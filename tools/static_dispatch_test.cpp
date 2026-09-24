// Verifica el despacho generado de extremo a extremo: publica por él con un
// DataWriter real y comprueba que un suscriptor recibe la muestra correcta.
//
// El probe de equivalencia CDR compara serialización sin red; esto comprueba
// que la fábrica, el TypeSupport y el write() están bien cableados. Son dos
// preguntas distintas y la segunda no se responde serializando en memoria.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "NmeaStaticDispatch.hpp"
#include "NmeaDBTPubSubTypes.hpp"

#include "mapper/Mapper.hpp"
#include "parser/Parser.hpp"
#include "registry/Registry.hpp"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>

using namespace nmea;
using namespace eprosima::fastdds::dds;

namespace {
int failures = 0;
void check(const char* name, bool ok, const std::string& detail) {
    std::printf("  [%s] %-44s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
    if (!ok) ++failures;
}

struct Recv : DataReaderListener {
    std::atomic<int> count{0};
    NmeaDBT last;
    void on_data_available(DataReader* r) override {
        NmeaDBT s; SampleInfo i;
        while (r->take_next_sample(&s, &i) == RETCODE_OK)
            if (i.valid_data) { last = s; count.fetch_add(1); }
    }
};

struct Parsed {
    Parser p; ParseResult r{ParseResult::Incomplete};
    explicit Parsed(std::string_view s) {
        for (char c : s) { r = p.consume(c); if (r != ParseResult::Incomplete) break; }
    }
    const SentenceView& v() const { return p.sentence(); }
};
}  // namespace

int main() {
    std::printf("despacho generado, de extremo a extremo\n\n");

    check("la fabrica devuelve un tipo para cada formatter de la campana",
          make_static_type("ROT") && make_static_type("RSA") && make_static_type("DBT")
                  && make_static_type("VHW") && make_static_type("GGA"),
          "ROT RSA DBT VHW GGA");
    check("la fabrica devuelve nullptr para un formatter sin tipo generado",
          make_static_type("MWV") == nullptr && make_static_type("") == nullptr,
          "el llamante tiene que poder decirlo, no callarlo");

    auto st = make_static_type("DBT");
    if (!st) { std::printf("sin tipo DBT\n"); return 1; }

    auto* factory = DomainParticipantFactory::get_instance();
    auto* part = factory->create_participant(197, PARTICIPANT_QOS_DEFAULT);
    if (!part) { std::printf("sin participante\n"); return 1; }

    TypeSupport ts = st->type_support();
    part->register_type(ts, "NmeaDBT");
    auto* topic = part->create_topic("nmea/sounder/DBT", "NmeaDBT", TOPIC_QOS_DEFAULT);

    DataWriterQos wq = DATAWRITER_QOS_DEFAULT;
    wq.reliability().kind = RELIABLE_RELIABILITY_QOS;   // para no perder la muestra
    auto* pub = part->create_publisher(PUBLISHER_QOS_DEFAULT);
    auto* w = pub->create_datawriter(topic, wq);

    Recv listener;
    DataReaderQos rq = DATAREADER_QOS_DEFAULT;
    rq.reliability().kind = RELIABLE_RELIABILITY_QOS;
    auto* sub = part->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    auto* rd = sub->create_datareader(topic, rq, &listener);

    check("writer y reader creados", w != nullptr && rd != nullptr, "dominio 197");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));   // emparejamiento

    // Brazas vacías: su bit debe llegar en cero al otro lado.
    Parsed p("$SDDBT,036.5,f,011.1,M,,F*29\r\n");
    const bool sent = st->write(w, p.v(), "sounder-1", "SD", 1700000000000000000LL);
    check("write por el despacho devuelve exito", sent, "");

    for (int i = 0; i < 40 && listener.count.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    check("el suscriptor recibio la muestra", listener.count.load() == 1,
          std::to_string(listener.count.load()) + " muestra(s)");

    if (listener.count.load() > 0) {
        const auto& s = listener.last;
        check("device_id llego intacto", s.device_id() == "sounder-1", s.device_id());
        check("los campos presentes llegaron con su valor",
              s.depth_feet() > 36.4 && s.depth_feet() < 36.6 && s.feet_unit() == 'f',
              "depth_feet=" + std::to_string(s.depth_feet()));
        // El bit 4 es depth_fathoms, que venia vacio.
        const bool fathoms_absent = (s.field_presence() & (1u << 4)) == 0;
        const bool feet_present = (s.field_presence() & (1u << 0)) != 0;
        check("la mascara distingue ausente de presente en el receptor",
              fathoms_absent && feet_present,
              // En decimal a proposito: la etiqueta decia 0x y el valor salia en
              // decimal, que es peor que no decir la base.
              "field_presence=" + std::to_string(s.field_presence()) +
                      " (bit 4, depth_fathoms, en cero)");
    }

    part->delete_contained_entities();
    factory->delete_participant(part);
    std::printf("\n%d verificacion(es) fallida(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
