// tools/dds_dynamic_sub.cpp
// Suscriptor Fast DDS INDEPENDIENTE: descubre el tipo remoto por XTypes
// (TypeObject propagado por el cable), reconstruye el DynamicType SIN ningún
// conocimiento del IDL/Registry del gateway, y vuelca las muestras como JSON.
// Es la prueba honesta de interoperabilidad a nivel de wire de un consumidor DDS
// nativo de terceros (mismo rol que Fast DDS Spy).
//   uso: dds_dynamic_sub <domain> <secs>   (def: 0 15)
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeBuilderFactory.hpp>
#include <fastdds/dds/xtypes/type_representation/ITypeObjectRegistry.hpp>
#include <fastdds/dds/xtypes/type_representation/TypeObject.hpp>
#include <fastdds/dds/xtypes/utils.hpp>
#include <fastdds/rtps/builtin/data/PublicationBuiltinTopicData.hpp>
#include <fastdds/rtps/reader/ReaderDiscoveryStatus.hpp>
#include <fastdds/rtps/writer/WriterDiscoveryStatus.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace eprosima::fastdds::dds;

static std::atomic<int> g_samples{0};

// Cola de tipos descubiertos: el reader se crea en el hilo main, NO dentro del
// callback de descubrimiento (crear entidades dentro de un listener de Fast DDS
// es un gotcha de re-entrancia que deja el reader sin emparejar).
struct Pending { std::string topic; std::string type; DynamicType::_ref_type dyn; };
static std::mutex          g_mu;
static std::vector<Pending> g_pending;

class DataListener : public DataReaderListener {
    DynamicType::_ref_type type_;
public:
    explicit DataListener(DynamicType::_ref_type t) : type_(t) {}
    void on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& s) override {
        std::cout << "[match] writers emparejados: " << s.current_count
                  << " (cambio " << s.current_count_change << ")\n";
    }
    void on_requested_incompatible_qos(DataReader*,
            const RequestedIncompatibleQosStatus& s) override {
        std::cout << "[QoS INCOMPATIBLE] policy_id=" << s.last_policy_id
                  << " total=" << s.total_count << "\n";
    }
    void on_data_available(DataReader* reader) override {
        std::cout << "[on_data_available] invocado\n";
        DynamicData::_ref_type data = DynamicDataFactory::get_instance()->create_data(type_);
        SampleInfo info;
        ReturnCode_t rc;
        while ((rc = reader->take_next_sample(&data, &info)) == RETCODE_OK) {
            if (!info.valid_data) continue;
            std::stringstream os;
            json_serialize(data, DynamicDataJsonFormat::EPROSIMA, os);
            std::cout << "  sample: " << os.str() << "\n";
            ++g_samples;
        }
        if (rc != RETCODE_NO_DATA && rc != RETCODE_OK)
            std::cout << "[take] retornó algo distinto de OK/NO_DATA\n";
    }
};

class PartListener : public DomainParticipantListener {
    std::set<std::string> seen_;
public:
    PartListener() = default;

    void on_data_writer_discovery(
            DomainParticipant*,
            eprosima::fastdds::rtps::WriterDiscoveryStatus reason,
            const eprosima::fastdds::dds::PublicationBuiltinTopicData& info,
            bool& should_be_ignored) override {
        should_be_ignored = false;
        using WDS = eprosima::fastdds::rtps::WriterDiscoveryStatus;
        if (reason != WDS::DISCOVERED_WRITER) return;

        const std::string topic = info.topic_name.to_string();
        const std::string type  = info.type_name.to_string();
        if (seen_.count(topic)) return;

        std::cout << "[writer rep] anuncia data_representation: ";
        if (info.representation.m_value.empty()) std::cout << "(vacío = XCDR1 por defecto)";
        for (auto r : info.representation.m_value)
            std::cout << (r == 0 ? "XCDR1 " : r == 2 ? "XCDR2 " : "? ");
        std::cout << "\n";

        // Recupera el TypeObject COMPLETO desde el registro local, poblado por
        // la propagación XTypes / TypeLookup desde el publicador remoto.
        const auto& tid =
            info.type_information.type_information.complete().typeid_with_size().type_id();
        xtypes::TypeObject type_obj;
        if (DomainParticipantFactory::get_instance()->type_object_registry()
                .get_type_object(tid, type_obj) != RETCODE_OK) {
            std::cout << "[" << topic << "] tipo '" << type
                      << "' NO recuperable: el TypeObject no se propagó por el cable\n";
            return;
        }
        auto builder = DynamicTypeBuilderFactory::get_instance()
                ->create_type_w_type_object(type_obj);
        if (!builder) { std::cout << "[" << topic << "] create_type_w_type_object falló\n"; return; }
        DynamicType::_ref_type dyn = builder->build();
        if (!dyn) { std::cout << "[" << topic << "] build() falló\n"; return; }

        seen_.insert(topic);
        std::cout << "\n[discovery] topic=" << topic << "  type=" << type
                  << "  -> DynamicType reconstruido SOLO desde el cable ✓\n";
        std::stringstream idl;
        idl_serialize(dyn, idl);
        std::cout << idl.str() << "\n";

        // Difiere la creación del reader al hilo main.
        std::lock_guard<std::mutex> lk(g_mu);
        g_pending.push_back({topic, type, dyn});
    }
};

int main(int argc, char** argv) {
    const int domain = argc > 1 ? std::atoi(argv[1]) : 0;
    const int secs   = argc > 2 ? std::atoi(argv[2]) : 15;

    auto* factory = DomainParticipantFactory::get_instance();
    DomainParticipant* part =
            factory->create_participant(domain, PARTICIPANT_QOS_DEFAULT);
    Subscriber* sub = part->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    part->set_listener(new PartListener());

    std::cout << "Suscriptor DDS independiente en dominio " << domain
              << " por " << secs << "s (sin IDL ni Registry del gateway)...\n";

    // Bucle main: crea los readers de los tipos descubiertos (fuera del callback)
    // y ADEMÁS los sondea activamente con take() (no depende del listener).
    std::vector<std::pair<DataReader*, DynamicType::_ref_type>> readers;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(secs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::vector<Pending> batch;
        { std::lock_guard<std::mutex> lk(g_mu); batch.swap(g_pending); }
        for (auto& p : batch) {
            TypeSupport ts(new DynamicPubSubType(p.dyn));
            ts.register_type(part, p.type);
            Topic* t = part->create_topic(p.topic, p.type, TOPIC_QOS_DEFAULT);
            DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
            rqos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
            DataReader* r = sub->create_datareader(t, rqos, new DataListener(p.dyn));
            readers.emplace_back(r, p.dyn);
            std::cout << "[reader] creado para " << p.topic << "\n";
        }
        // Sondeo activo.
        for (auto& [r, dyn] : readers) {
            DynamicData::_ref_type data = DynamicDataFactory::get_instance()->create_data(dyn);
            SampleInfo si;
            while (r->take_next_sample(&data, &si) == RETCODE_OK) {
                if (!si.valid_data) continue;
                std::stringstream os;
                json_serialize(data, DynamicDataJsonFormat::EPROSIMA, os);
                std::cout << "  [poll] " << os.str() << "\n";
                ++g_samples;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n--- Resultado ---\nsamples decodificados desde el cable: "
              << g_samples.load() << "\n"
              << (g_samples > 0 ? "PASS ✓ interop XTypes wire-level confirmada"
                                : "FAIL ✗ sin datos decodificados") << "\n";
    return g_samples > 0 ? 0 : 1;
}
