// src/ui/GatewayController.cpp
#include "ui/GatewayController.hpp"

#include <QMetaObject>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <functional>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/builtin/topic/PublicationBuiltinTopicData.hpp>
#include <fastdds/dds/builtin/topic/SubscriptionBuiltinTopicData.hpp>
#include <fastdds/rtps/writer/WriterDiscoveryStatus.hpp>
#include <fastdds/rtps/reader/ReaderDiscoveryStatus.hpp>

#include "capture/SerialSource.hpp"
#include "capture/TcpSource.hpp"

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
    stopPreview();
    stopScan();
    for (auto& [id, p] : pipelines_) p->stop();
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
    auto src = std::make_unique<nmea::SerialSource>();
    if (!src->open(source.toStdString(), baud)) return nullptr;
    return src;
}

void GatewayController::startPreview(const QString& source, int baud,
                                      const QString& deviceId) {
    stopPreview();
    auto src = makeSource(source, baud);
    if (!src) {
        emit networkDiagResult("Conexión serial/TCP", false,
                               "No se pudo abrir " + source);
        return;
    }
    nmea::Pipeline::Config cfg;
    cfg.device_id      = deviceId.toStdString();
    cfg.source         = std::move(src);
    cfg.registry       = &registry_;
    cfg.domain_id      = 0;
    cfg.publish_to_dds = false;
    cfg.on_sentence = [this](std::string formatter, std::string category,
                              std::vector<std::string> names,
                              std::vector<std::string> values) {
        QStringList qnames, qvalues;
        for (auto& n : names)  qnames  << QString::fromStdString(n);
        for (auto& v : values) qvalues << QString::fromStdString(v);
        const QString fmt = QString::fromStdString(formatter);
        const QString cat = QString::fromStdString(category);
        QMetaObject::invokeMethod(this, [this, fmt, cat, qnames, qvalues]() {
            auto& rt = rate_trackers_[fmt.toStdString()];
            rt.last_count++;
            emit sentenceDetected(fmt, cat, qnames, qvalues, rt.rate_hz);
        }, Qt::QueuedConnection);
    };
    preview_pipeline_ = std::make_unique<nmea::Pipeline>(std::move(cfg));
    preview_pipeline_->start();
}

void GatewayController::stopPreview() {
    if (preview_pipeline_) {
        preview_pipeline_->stop();
        preview_pipeline_.reset();
    }
    rate_trackers_.clear();
}

void GatewayController::launchConversion(const QString& source, int baud,
                                          const QString& deviceId, int domainId,
                                          const QoSProfile& qos) {
    stopPreview();
    auto src = makeSource(source, baud);
    if (!src) return;
    nmea::Pipeline::Config cfg;
    cfg.device_id      = deviceId.toStdString();
    cfg.source         = std::move(src);
    cfg.registry       = &registry_;
    cfg.domain_id      = domainId;
    cfg.publish_to_dds = true;
    // Aplica el perfil QoS elegido en la UI al DataWriter (D8).
    cfg.qos = nmea::Pipeline::QosSettings{
        qos.reliable, qos.transient_local, qos.deadline_ms, qos.lifespan_ms};
    auto pipeline = std::make_unique<nmea::Pipeline>(std::move(cfg));
    pipeline->start();
    pipelines_[deviceId.toStdString()] = std::move(pipeline);
}

void GatewayController::stopConversion(const QString& deviceId) {
    auto it = pipelines_.find(deviceId.toStdString());
    if (it == pipelines_.end()) return;
    it->second->stop();
    pipelines_.erase(it);
    emit conversionStateChanged(deviceId, 0, 0);
}

void GatewayController::pollPipelines() {
    for (auto& [id, p] : pipelines_) {
        const int state = static_cast<int>(p->state());
        emit conversionStateChanged(QString::fromStdString(id),
                                    state, p->sentences_ok());
    }
    for (auto& [fmt, rt] : rate_trackers_) {
        rt.rate_hz = static_cast<double>(rt.last_count - rt.prev_count) * 10.0;
        rt.prev_count = rt.last_count;
    }
}

void GatewayController::runNetworkDiagnostics() {
    {
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        bool ok = false;
        if (s >= 0) {
            struct ip_mreq mreq{};
            mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");
            mreq.imr_interface.s_addr = INADDR_ANY;
            ok = (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == 0);
            if (ok) setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
            ::close(s);
        }
        emit networkDiagResult("Multicast SPDP 239.255.0.1", ok,
                               ok ? "Grupo multicast accesible" : "No se pudo unir al grupo multicast");
    }
    {
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        bool ok = false;
        if (s >= 0) {
            struct sockaddr_in addr{};
            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port        = htons(7400);
            int reuseaddr = 1;
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
            ok = (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
            ::close(s);
        }
        emit networkDiagResult("Puerto UDP 7400 (RTPS base)", ok,
                               ok ? "Puerto disponible" : "Puerto bloqueado o en uso");
    }
    {
        const char* env = std::getenv("FASTRTPS_DEFAULT_PROFILES_FILE");
        const bool ok = (env != nullptr && env[0] != '\0');
        emit networkDiagResult("FASTRTPS_DEFAULT_PROFILES_FILE", ok,
                               ok ? QString("Apunta a: ") + env : "Variable no definida (se usan defaults)");
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
