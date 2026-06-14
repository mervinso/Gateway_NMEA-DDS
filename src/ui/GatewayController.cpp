// src/ui/GatewayController.cpp
#include "ui/GatewayController.hpp"

#include <QMetaObject>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <thread>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/builtin/topic/PublicationBuiltinTopicData.hpp>
#include <fastdds/dds/builtin/topic/SubscriptionBuiltinTopicData.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeMember.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>
#include <fastdds/rtps/writer/WriterDiscoveryStatus.hpp>
#include <fastdds/rtps/reader/ReaderDiscoveryStatus.hpp>

#include "capture/SerialSource.hpp"
#include "capture/TcpSource.hpp"
#include "capture/UdpSource.hpp"
#include "mapper/Mapper.hpp"

namespace nmea::ui {

// Listener de descubrimiento DDS. Los callbacks corren en hilos de Fast DDS:
// solo invocan `cb` (un std::function neutral); el marshalling a Qt lo hace
// GatewayController dentro de `cb`.
class MonitorListener : public eprosima::fastdds::dds::DomainParticipantListener {
public:
    // (isWriter, added, topic, type)
    std::function<void(bool, bool, std::string, std::string)> cb;

    void on_data_writer_discovery(
            eprosima::fastdds::dds::DomainParticipant*,
            eprosima::fastdds::rtps::WriterDiscoveryStatus reason,
            const eprosima::fastdds::dds::PublicationBuiltinTopicData& info,
            bool& should_be_ignored) override {
        should_be_ignored = false;
        using WDS = eprosima::fastdds::rtps::WriterDiscoveryStatus;
        if (reason == WDS::DISCOVERED_WRITER || reason == WDS::REMOVED_WRITER) {
            if (cb) cb(true, reason == WDS::DISCOVERED_WRITER,
                       info.topic_name.c_str(), info.type_name.c_str());
        }
    }

    void on_data_reader_discovery(
            eprosima::fastdds::dds::DomainParticipant*,
            eprosima::fastdds::rtps::ReaderDiscoveryStatus reason,
            const eprosima::fastdds::dds::SubscriptionBuiltinTopicData& info,
            bool& should_be_ignored) override {
        should_be_ignored = false;
        using RDS = eprosima::fastdds::rtps::ReaderDiscoveryStatus;
        if (reason == RDS::DISCOVERED_READER || reason == RDS::REMOVED_READER) {
            if (cb) cb(false, reason == RDS::DISCOVERED_READER,
                       info.topic_name.c_str(), info.type_name.c_str());
        }
    }
};

GatewayController::GatewayController(QObject* parent)
    : QObject(parent)
    , registry_(nmea::Registry::builtin())
    , poll_timer_(new QTimer(this))
{
    connect(poll_timer_, &QTimer::timeout, this, &GatewayController::pollPipelines);
    poll_timer_->start(100);  // 10 Hz
}

GatewayController::~GatewayController() {
    disconnectAll();
    stopScan();
}

std::unique_ptr<nmea::ISource> GatewayController::makeSource(
        const QString& source, int baud) {
    if (source.startsWith("tcp://")) {
        const QString addr = source.mid(6);
        const int colon = addr.lastIndexOf(':');
        if (colon < 0) return nullptr;
        const QString host = addr.left(colon);
        const uint16_t port = addr.mid(colon+1).toUShort();
        auto src = std::make_unique<nmea::TcpSource>();
        if (!src->connect(host.toStdString(), port)) return nullptr;
        return src;
    }
    if (source.startsWith("udp://")) {
        // "udp://3100" o "udp://:3100" → bind al puerto local para recibir broadcast.
        QString rest = source.mid(6);
        const int colon = rest.lastIndexOf(':');
        if (colon >= 0) rest = rest.mid(colon + 1);
        const uint16_t port = rest.toUShort();
        if (port == 0) return nullptr;
        auto src = std::make_unique<nmea::UdpSource>();
        if (!src->open(port)) return nullptr;
        return src;
    }
    auto src = std::make_unique<nmea::SerialSource>();
    if (!src->open(source.toStdString(), baud)) return nullptr;
    return src;
}

void GatewayController::connectInterface(const QString& source, int baud,
                                          int domainId) {
    if (pipelines_.count(source)) {
        emit interfaceError(source, "La interfaz ya está conectada");
        return;
    }
    auto src = makeSource(source, baud);
    if (!src) {
        emit interfaceError(source, "No se pudo abrir " + source);
        return;
    }
    nmea::Pipeline::Config cfg;
    cfg.source         = std::move(src);
    cfg.registry       = &registry_;
    cfg.domain_id      = domainId;
    cfg.publish_to_dds = true;       // el plan decide qué se publica (arranca vacío)
    cfg.plan           = &publish_plan_;
    cfg.on_sentence = [this, source](std::string talker, std::string formatter,
                              std::string category,
                              std::vector<std::string> names,
                              std::vector<std::string> values) {
        QStringList qnames, qvalues;
        for (auto& n : names)  qnames  << QString::fromStdString(n);
        for (auto& v : values) qvalues << QString::fromStdString(v);
        const QString tk  = QString::fromStdString(talker);
        const QString fmt = QString::fromStdString(formatter);
        const QString cat = QString::fromStdString(category);
        QMetaObject::invokeMethod(this, [this, source, tk, fmt, cat, qnames, qvalues]() {
            auto& rt = rate_trackers_[(tk + "|" + fmt).toStdString()];
            rt.last_count++;
            iface_rates_[source].last_count++;
            emit sentenceDetected(source, tk, fmt, cat, qnames, qvalues, rt.rate_hz);
        }, Qt::QueuedConnection);
    };
    auto pipe = std::make_unique<nmea::Pipeline>(std::move(cfg));
    pipe->start();
    pipelines_[source] = std::move(pipe);
    emit interfaceConnected(source);
}

void GatewayController::disconnectInterface(const QString& source) {
    auto it = pipelines_.find(source);
    if (it == pipelines_.end()) return;
    it->second->stop();
    pipelines_.erase(it);
    iface_rates_.erase(source);
    emit interfaceDisconnected(source);
}

void GatewayController::disconnectAll() {
    for (auto& [src, pipe] : pipelines_) pipe->stop();
    pipelines_.clear();
    rate_trackers_.clear();
    iface_rates_.clear();
}

namespace {
QString qosLabel(const QoSProfile& p) {
    QString s;
    if (!p.name.empty()) s += QString::fromStdString(p.name) + " · ";
    s += p.reliable ? "RELIABLE" : "BEST_EFFORT";
    s += p.transient_local ? "/TL" : "/VOL";
    if (p.deadline_ms > 0) s += QString(" d%1").arg(p.deadline_ms);
    if (p.lifespan_ms > 0) s += QString(" l%1").arg(p.lifespan_ms);
    return s;
}
}  // namespace

void GatewayController::addConversion(const QString& talker,
                                       const QString& formatter,
                                       const QString& deviceId,
                                       const QoSProfile& qos) {
    publish_plan_.add(talker.toStdString(), formatter.toStdString(),
                      deviceId.toStdString(),
                      nmea::Pipeline::QosSettings{qos.reliable, qos.transient_local,
                                                  qos.deadline_ms, qos.lifespan_ms});
    nmea::Mapper mapper(registry_);
    const auto info = mapper.resolve((talker + formatter).toStdString());
    emit conversionAdded(talker, formatter, deviceId,
                         QString::fromStdString(info.topic_name), qosLabel(qos));
}

QoSProfile GatewayController::conversionQoS(const QString& talker,
                                            const QString& formatter) const {
    const auto t = publish_plan_.resolve(talker.toStdString(), formatter.toStdString());
    if (!t) return {};
    return QoSProfile{"", t->qos.reliable, t->qos.transient_local,
                      t->qos.deadline_ms, t->qos.lifespan_ms};
}

void GatewayController::updateConversionQoS(const QString& talker,
                                            const QString& formatter,
                                            const QoSProfile& qos) {
    const auto t = publish_plan_.resolve(talker.toStdString(), formatter.toStdString());
    if (!t) return;
    publish_plan_.add(talker.toStdString(), formatter.toStdString(), t->device_id,
                      nmea::Pipeline::QosSettings{qos.reliable, qos.transient_local,
                                                  qos.deadline_ms, qos.lifespan_ms});
    emit conversionQoSChanged(talker, formatter, qosLabel(qos));
}

void GatewayController::removeConversion(const QString& talker,
                                          const QString& formatter) {
    publish_plan_.remove(talker.toStdString(), formatter.toStdString());
    emit conversionRemoved(talker, formatter);
}

void GatewayController::pollPipelines() {
    for (auto& [fmt, rt] : rate_trackers_) {
        rt.rate_hz = static_cast<double>(rt.last_count - rt.prev_count) * 10.0;
        rt.prev_count = rt.last_count;
    }
    for (auto& [src, rt] : iface_rates_) {
        rt.rate_hz = static_cast<double>(rt.last_count - rt.prev_count) * 10.0;
        rt.prev_count = rt.last_count;
        emit interfaceRate(src, rt.rate_hz);
    }
}

void GatewayController::scanDomain(int domainId) {
    using namespace eprosima::fastdds::dds;
    stopScan();
    topic_agg_.clear();
    monitor_domain_ = domainId;

    monitor_listener_ = std::make_unique<MonitorListener>();
    monitor_listener_->cb = [this](bool isWriter, bool added,
                                    std::string topic, std::string type) {
        QMetaObject::invokeMethod(this, [this, isWriter, added, topic, type]() {
            auto& agg = topic_agg_[topic];
            if (!type.empty()) agg.typeName = type;
            const int delta = added ? 1 : -1;
            if (isWriter) agg.pub = std::max(0, agg.pub + delta);
            else          agg.sub = std::max(0, agg.sub + delta);
            emit ddsTopicDiscovered(monitor_domain_,
                    QString::fromStdString(topic),
                    QString::fromStdString(agg.typeName),
                    agg.pub, agg.sub);
        }, Qt::QueuedConnection);
    };

    monitor_participant_ = DomainParticipantFactory::get_instance()->create_participant(
            domainId, PARTICIPANT_QOS_DEFAULT, monitor_listener_.get(), StatusMask::none());

    if (!monitor_participant_) {
        monitor_listener_.reset();
        emit networkDiagResult("Monitor DDS", false,
                QString("No se pudo crear participante en dominio %1").arg(domainId));
        return;
    }
    emit networkDiagResult("Monitor DDS", true,
            QString("Escuchando dominio %1…").arg(domainId));
}

// Formatea un DynamicData como "campo = valor" por línea, leyendo cada miembro
// según su TypeKind.
static QString dynamicDataToString(
        eprosima::fastdds::dds::DynamicType::_ref_type type,
        eprosima::fastdds::dds::DynamicData::_ref_type data) {
    using namespace eprosima::fastdds::dds;
    QString out;
    DynamicTypeMembersById members;
    type->get_all_members(members);
    for (auto& [id, member] : members) {
        MemberDescriptor::_ref_type desc = traits<MemberDescriptor>::make_shared();
        member->get_descriptor(desc);
        const QString name = QString::fromStdString(std::string(desc->name()));
        const TypeKind kind = desc->type() ? desc->type()->get_kind() : TK_NONE;
        QString val;
        switch (kind) {
            case TK_STRING8: { std::string s; data->get_string_value(s, id);  val = QString::fromStdString(s); break; }
            case TK_FLOAT64: { double v;      data->get_float64_value(v, id); val = QString::number(v); break; }
            case TK_FLOAT32: { float v;       data->get_float32_value(v, id); val = QString::number(v); break; }
            case TK_INT32:   { int32_t v;     data->get_int32_value(v, id);   val = QString::number(v); break; }
            case TK_UINT32:  { uint32_t v;    data->get_uint32_value(v, id);  val = QString::number(v); break; }
            case TK_INT64:   { int64_t v;     data->get_int64_value(v, id);   val = QString::number(qlonglong(v)); break; }
            case TK_CHAR8:   { char v;        data->get_char8_value(v, id);   val = QString(QChar(v)); break; }
            default:         val = "?";
        }
        out += name + " = " + val + "\n";
    }
    return out;
}

QString GatewayController::readTopicSample(const QString& topicName,
                                           const QString& typeName) {
    using namespace eprosima::fastdds::dds;
    if (!monitor_participant_)
        return "Inicia un barrido en el Monitor antes de leer un sample.";

    // Reconstruye el DynamicType desde el registro (solo tipos del gateway).
    const std::string tn = typeName.toStdString();
    Mapper mapper(registry_);
    DynamicType::_ref_type dyn_type;
    if (tn == "RawSentence") {
        dyn_type = mapper.raw_sentence_type();
    } else if (tn.rfind("Nmea", 0) == 0) {
        dyn_type = mapper.type_for(tn.substr(4));
    } else {
        return QString("Tipo '%1' no reconocido: solo se pueden leer tópicos "
                       "publicados por este gateway.").arg(typeName);
    }
    if (!dyn_type) return "No se pudo reconstruir el tipo dinámico.";

    TypeSupport ts(new DynamicPubSubType(dyn_type));
    ts.register_type(monitor_participant_, tn);

    Topic* topic = monitor_participant_->create_topic(
            topicName.toStdString(), tn, TOPIC_QOS_DEFAULT);
    if (!topic) return "No se pudo crear el tópico para lectura.";

    Subscriber* sub = monitor_participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    DataReaderQos rq = DATAREADER_QOS_DEFAULT;
    // BEST_EFFORT casa con writers BEST_EFFORT y RELIABLE (ofrecido ≥ pedido).
    rq.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
    DataReader* reader = sub ? sub->create_datareader(topic, rq) : nullptr;

    QString result = "Sin datos en 3 s. ¿Hay una conversión publicando "
                     "este tópico ahora mismo?";
    if (!reader) {
        result = "No se pudo crear el lector.";
    } else {
        // Sondea take_next_sample hasta ~3 s (wait_for_unread_message no es
        // fiable aquí; el sondeo sí entrega la muestra).
        for (int i = 0; i < 60; ++i) {
            DynamicData::_ref_type sample =
                    DynamicDataFactory::get_instance()->create_data(dyn_type);
            SampleInfo info;
            if (reader->take_next_sample(&sample, &info) == RETCODE_OK && info.valid_data) {
                result = dynamicDataToString(dyn_type, sample);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    if (sub) {
        if (reader) sub->delete_datareader(reader);
        monitor_participant_->delete_subscriber(sub);
    }
    monitor_participant_->delete_topic(topic);
    return result;
}

void GatewayController::stopScan() {
    if (monitor_participant_) {
        eprosima::fastdds::dds::DomainParticipantFactory::get_instance()
                ->delete_participant(monitor_participant_);
        monitor_participant_ = nullptr;
    }
    monitor_listener_.reset();
    topic_agg_.clear();
}

}  // namespace nmea::ui
