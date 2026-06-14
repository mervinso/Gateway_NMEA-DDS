// tools/verify_live.cpp — verificación end-to-end contra el simulador NMEA en vivo.
// Reproduce exactamente lo que GatewayController::connectInterface + addConversion
// construyen (Pipeline con PublishPlan), pero sin GUI, y observa el tópico DDS
// publicado con un DataReader real en el dominio 0.
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "capture/SerialSource.hpp"
#include "mapper/Mapper.hpp"
#include "pipeline/Pipeline.hpp"
#include "pipeline/PublishPlan.hpp"
#include "registry/Registry.hpp"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeMember.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>

using namespace eprosima::fastdds::dds;
using namespace nmea;

static std::string dump(DynamicType::_ref_type type, DynamicData::_ref_type data) {
    std::string out;
    DynamicTypeMembersById members;
    type->get_all_members(members);
    for (auto& [id, member] : members) {
        MemberDescriptor::_ref_type desc = traits<MemberDescriptor>::make_shared();
        member->get_descriptor(desc);
        const std::string name = std::string(desc->name());
        const TypeKind kind = desc->type() ? desc->type()->get_kind() : TK_NONE;
        std::string val;
        switch (kind) {
            case TK_STRING8: { std::string s; data->get_string_value(s, id); val = s; break; }
            case TK_FLOAT64: { double v; data->get_float64_value(v, id); val = std::to_string(v); break; }
            case TK_FLOAT32: { float v; data->get_float32_value(v, id); val = std::to_string(v); break; }
            case TK_INT32:   { int32_t v; data->get_int32_value(v, id); val = std::to_string(v); break; }
            case TK_UINT32:  { uint32_t v; data->get_uint32_value(v, id); val = std::to_string(v); break; }
            case TK_INT64:   { int64_t v; data->get_int64_value(v, id); val = std::to_string(v); break; }
            case TK_CHAR8:   { char v; data->get_char8_value(v, id); val = std::string(1, v); break; }
            default: val = "?";
        }
        out += "    " + name + " = " + val + "\n";
    }
    return out;
}

int main() {
    Registry reg = Registry::builtin();

    // --- Lector DDS real en dominio 0 para el tópico que se publicará ---
    const std::string topic_name = "nmea/inertial/VNYPR";
    const std::string type_name  = "NmeaVNYPR";
    Mapper mapper(reg);
    DynamicType::_ref_type dyn_type = mapper.type_for("VNYPR");
    DomainParticipant* part = DomainParticipantFactory::get_instance()
            ->create_participant(0, PARTICIPANT_QOS_DEFAULT);
    TypeSupport ts(new DynamicPubSubType(dyn_type));
    ts.register_type(part, type_name);
    Topic* topic = part->create_topic(topic_name, type_name, TOPIC_QOS_DEFAULT);
    Subscriber* sub = part->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    DataReaderQos rq = DATAREADER_QOS_DEFAULT;
    rq.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
    DataReader* reader = sub->create_datareader(topic, rq);

    // --- Recolector de sentencias (lo que DevicesPanel agruparía) ---
    std::mutex mu;
    std::map<std::string, int> counts;           // "talker|formatter" -> n
    std::map<std::string, std::string> cats;     // "talker|formatter" -> category

    PublishPlan plan;

    auto src = std::make_unique<SerialSource>();
    if (!src->open("/dev/ttyUSB0", 115200)) {
        std::cerr << "FALLO: no se pudo abrir /dev/ttyUSB0\n";
        return 2;
    }

    Pipeline::Config cfg;
    cfg.source         = std::move(src);
    cfg.registry       = &reg;
    cfg.domain_id      = 0;
    cfg.publish_to_dds = true;
    cfg.plan           = &plan;
    cfg.on_sentence = [&](std::string talker, std::string formatter,
                          std::string category, std::vector<std::string>,
                          std::vector<std::string>) {
        if (formatter.empty()) return;
        std::lock_guard<std::mutex> lk(mu);
        const std::string k = talker + "|" + formatter;
        counts[k]++;
        cats[k] = category;
    };

    Pipeline pipe(std::move(cfg));
    pipe.start();

    std::cout << "[1] Ingesta de sensor (USB /dev/ttyUSB0), 4 s, plan VACÍO...\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));

    std::cout << "\n=== Sensores → Tramas detectados (lo que ② agruparía) ===\n";
    {
        std::lock_guard<std::mutex> lk(mu);
        std::set<std::string> talkers;
        for (auto& [k, n] : counts) talkers.insert(k.substr(0, k.find('|')));
        for (auto& t : talkers) {
            std::cout << "📡 Sensor talker='" << (t.empty() ? "(prop)" : t) << "'\n";
            for (auto& [k, n] : counts) {
                if (k.substr(0, k.find('|')) != t) continue;
                const std::string fmt = k.substr(k.find('|') + 1);
                std::cout << "     └─ " << fmt << "  [" << cats[k]
                          << "]  x" << n << "\n";
            }
        }
    }

    std::cout << "\n[2] addConversion(VN, YPR, device_id=imu_vn100, BEST_EFFORT) "
                 "→ publicación SELECTIVA\n";
    plan.add("VN", "YPR", "imu_vn100", Pipeline::QosSettings{true, false, 0, 0});
    plan.add("", "VNYPR", "imu_vn100", Pipeline::QosSettings{true, false, 0, 0});

    std::cout << "[3] Leyendo el tópico " << topic_name
              << " del bus DDS (dominio 0)...\n";
    bool got = false;
    for (int i = 0; i < 120 && !got; ++i) {
        DynamicData::_ref_type sample =
                DynamicDataFactory::get_instance()->create_data(dyn_type);
        SampleInfo info;
        if (reader->take_next_sample(&sample, &info) == RETCODE_OK && info.valid_data) {
            std::cout << "\n=== ✅ SAMPLE recibido en " << topic_name << " ===\n"
                      << dump(dyn_type, sample);
            got = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!got) std::cout << "\n=== ❌ Sin sample en 6 s ===\n";

    // ¿Se publica SOLO lo habilitado? Comprobamos que un formatter NO añadido
    // (p.ej. GLL) no tenga writer: lo verificamos leyendo su tópico brevemente.
    std::cout << "\n[4] Control negativo: leer nmea/gps/GLL (NO convertido) 2 s...\n";
    {
        DynamicType::_ref_type gll_t = mapper.type_for("GLL");
        TypeSupport ts2(new DynamicPubSubType(gll_t));
        ts2.register_type(part, "NmeaGLL");
        Topic* gtopic = part->create_topic("nmea/gps/GLL", "NmeaGLL", TOPIC_QOS_DEFAULT);
        DataReader* greader = sub->create_datareader(gtopic, rq);
        bool gll = false;
        for (int i = 0; i < 40 && !gll; ++i) {
            DynamicData::_ref_type s =
                    DynamicDataFactory::get_instance()->create_data(gll_t);
            SampleInfo gi;
            if (greader->take_next_sample(&s, &gi) == RETCODE_OK && gi.valid_data) gll = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::cout << (gll ? "  ⚠️ INESPERADO: GLL publica sin estar convertido\n"
                          : "  ✅ Correcto: GLL NO publica (no está en el plan)\n");
        sub->delete_datareader(greader);
        part->delete_topic(gtopic);
    }

    std::cout << "\n[5] sentencias OK procesadas por el pipeline: "
              << pipe.sentences_ok() << " (err: " << pipe.sentences_err() << ")\n";

    pipe.stop();
    sub->delete_datareader(reader);
    part->delete_subscriber(sub);
    part->delete_topic(topic);
    DomainParticipantFactory::get_instance()->delete_participant(part);
    return got ? 0 : 1;
}
