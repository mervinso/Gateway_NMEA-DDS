// tools/arm_selector_test.cpp — los dos brazos, por el mismo Pipeline.
//
// Solo se construye con -DGATEWAY_STATIC_ARM=ON.
//
// La sonda de equivalencia CDR compara serialización, y el test de despacho
// comprueba el cableado de un DataWriter suelto. Esto comprueba lo que ninguno
// de los dos: que el **selector** funciona, que el mismo `Pipeline` alimentado
// por la misma fuente publica por el brazo que se le pidió, y que lo que llega
// al suscriptor es lo mismo en los dos casos.
//
// Es la forma que tendrá la compuerta 1 de la campaña, en pequeño: mismas
// sentencias a los dos brazos, comparar lo recibido.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "NmeaDBTPubSubTypes.hpp"
#include "capture/UdpSource.hpp"
#include "pipeline/Pipeline.hpp"
#include "registry/Registry.hpp"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
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
    std::printf("  [%s] %-46s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
    if (!ok) ++failures;
}

struct Collector : DataReaderListener {
    std::vector<NmeaDBT> got;
    std::atomic<int> n{0};
    void on_data_available(DataReader* r) override {
        NmeaDBT s; SampleInfo i;
        while (r->take_next_sample(&s, &i) == RETCODE_OK)
            if (i.valid_data) { got.push_back(s); n.fetch_add(1); }
    }
};

// La sentencia lleva las brazas vacías: el caso que la máscara distingue.
std::string sentence() { return "$SDDBT,036.5,f,011.1,M,,F*29\r\n"; }

// Corre un Pipeline en el brazo pedido y devuelve lo que el suscriptor recibió.
std::vector<NmeaDBT> run_arm(Pipeline::Typing typing, std::uint16_t port, int domain,
                             int n_sentences) {
    const Registry reg = Registry::builtin();

    auto* factory = DomainParticipantFactory::get_instance();
    auto* part = factory->create_participant(domain, PARTICIPANT_QOS_DEFAULT);
    TypeSupport ts(new NmeaDBTPubSubType());
    part->register_type(ts, "NmeaDBT");
    auto* topic = part->create_topic("nmea/sounder/DBT", "NmeaDBT", TOPIC_QOS_DEFAULT);
    Collector col;
    DataReaderQos rq = DATAREADER_QOS_DEFAULT;
    rq.reliability().kind = RELIABLE_RELIABILITY_QOS;
    rq.history().depth = 100;
    auto* sub = part->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    sub->create_datareader(topic, rq, &col);

    auto src = std::make_unique<UdpSource>();
    src->open(port);

    Pipeline::Config cfg;
    cfg.typing = typing;
    cfg.device_id = "sounder-1";
    cfg.source = std::move(src);
    cfg.registry = &reg;
    cfg.domain_id = domain;
    cfg.publish_to_dds = true;
    cfg.qos.reliable = true;          // para no perder muestras en la comparacion

    Pipeline pipe(std::move(cfg));
    pipe.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1800));

    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    to.sin_addr.s_addr = inet_addr("127.0.0.1");
    const std::string s = sentence();
    for (int i = 0; i < n_sentences; ++i) {
        ::sendto(fd, s.data(), s.size(), 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    ::close(fd);
    pipe.stop();

    auto out = col.got;
    part->delete_contained_entities();
    factory->delete_participant(part);
    return out;
}

bool same(const NmeaDBT& a, const NmeaDBT& b, std::string& why) {
    if (a.device_id() != b.device_id()) { why = "device_id"; return false; }
    if (a.talker() != b.talker()) { why = "talker: '" + a.talker() + "' vs '" + b.talker() + "'"; return false; }
    if (a.field_presence() != b.field_presence()) {
        why = "field_presence " + std::to_string(a.field_presence()) + " vs " +
              std::to_string(b.field_presence());
        return false;
    }
    if (a.depth_feet() != b.depth_feet()) { why = "depth_feet"; return false; }
    if (a.feet_unit() != b.feet_unit()) { why = "feet_unit"; return false; }
    if (a.depth_meters() != b.depth_meters()) { why = "depth_meters"; return false; }
    if (a.meters_unit() != b.meters_unit()) { why = "meters_unit"; return false; }
    if (a.depth_fathoms() != b.depth_fathoms()) { why = "depth_fathoms"; return false; }
    if (a.fathoms_unit() != b.fathoms_unit()) { why = "fathoms_unit"; return false; }
    return true;
}

}  // namespace

int main() {
    std::printf("selector de brazo: el mismo Pipeline por los dos caminos\n\n");
    constexpr int N = 20;

    const auto dyn = run_arm(Pipeline::Typing::Dynamic, 34601, 191, N);
    check("el brazo dinamico publico", static_cast<int>(dyn.size()) == N,
          std::to_string(dyn.size()) + " de " + std::to_string(N));

    const auto sta = run_arm(Pipeline::Typing::Static, 34602, 192, N);
    check("el brazo estatico publico", static_cast<int>(sta.size()) == N,
          std::to_string(sta.size()) + " de " + std::to_string(N));

    if (!dyn.empty() && !sta.empty()) {
        std::string why;
        const bool eq = same(dyn.front(), sta.front(), why);
        check("los dos brazos entregan la misma muestra", eq, eq ? "todos los miembros" : why);

        // Lo que el suscriptor ve del brazo estatico tiene que ser interpretable
        // sin conocer el brazo: la mascara con las brazas ausentes.
        const auto& s = sta.front();
        check("el brazo estatico entrega la mascara correcta",
              (s.field_presence() & (1u << 4)) == 0 && (s.field_presence() & 1u) != 0,
              "field_presence=" + std::to_string(s.field_presence()) +
                      ", bit 4 (depth_fathoms) en cero");
    }

    std::printf("\n%d verificacion(es) fallida(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
