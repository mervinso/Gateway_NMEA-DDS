// tools/measurement_run.cpp — una corrida de medición.
//
// Obligación 4b. Ejecuta exactamente una celda del diseño: un brazo, un
// formatter, un régimen, y escribe lo que solo este proceso sabe. Lo que es
// estado del sistema —frecuencia, térmica, aislamiento, tshark— lo recoge el
// arnés en Python, que es quien orquesta.
//
// Solo se construye con -DGATEWAY_INSTRUMENT=ON: sin la sonda no hay nada que
// medir.
//
// **El asentamiento se archiva, no se descarta.** El plan dice que las muestras
// de asentamiento se guardan aparte, así que la ventana empieza volcando lo
// acumulado a `warmup.csv` y limpiando la sonda. Descartarlas sería perder la
// evidencia de que el asentamiento hizo lo que se supone.
//
// Los transportes se piden explícitos —UDPv4, sin memoria compartida— porque
// `v3_transport` es criterio de anulación y la variable de entorno no se puede
// leer de vuelta. El manifiesto parcial registra lo que el participante tiene
// de verdad.

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture/UdpSource.hpp"
#include "instrument/PublishProbe.hpp"
#include "pipeline/Pipeline.hpp"
#include "registry/Registry.hpp"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>
#include <fastdds/config.hpp>
#include <fastcdr/config.h>

#include "mapper/Mapper.hpp"

using namespace nmea;
using namespace eprosima::fastdds::dds;

namespace {

struct Args {
    std::string typing{"dynamic"};
    std::string formatter{"DBT"};
    std::string regime{"normative"};
    std::string run_id{"run"};
    std::string out{"."};
    double rate{6.0};          // sentencias/s; 0 = tan rápido como se pueda
    double window_s{300.0};
    double settle_s{30.0};
    int    fanout{1};
    int    domain{170};
    std::uint16_t port{35100};
};

// Checksum NMEA: XOR de todo entre '$' y '*'. Se calcula, nunca se inventa: tres
// intentos se perdieron a mano en este proyecto por escribirlos de memoria.
std::string sentence_for(const std::string& formatter) {
    std::string body;
    if (formatter == "ROT")      body = "HEROT,-045.7,A";
    else if (formatter == "RSA") body = "IIRSA,-012.3,A,+004.5,A";
    else if (formatter == "DBT") body = "SDDBT,036.5,f,011.1,M,006.0,F";
    else if (formatter == "VHW") body = "VDVHW,045.0,T,043.2,M,010.5,N,019.4,K";
    else if (formatter == "GGA")
        body = "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,";
    else return {};
    unsigned ck = 0;
    for (char c : body) ck ^= static_cast<unsigned char>(c);
    char out[192];
    std::snprintf(out, sizeof out, "$%s*%02X\r\n", body.c_str(), ck);
    return out;
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o;
}

// SHA-256 (FIPS 180-4), autocontenido. El artefacto no enlaza ninguna
// biblioteca de criptografia y no merece la pena anadir la dependencia por el
// hash de un archivo de unos kilobytes que se lee una vez por corrida. No es
// codigo sensible a la velocidad; es codigo que tiene que estar bien, asi que
// se comprueba contra `sha256sum` en la verificacion.
struct Sha256 {
    static constexpr std::uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
        0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
        0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
        0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
        0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
        0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
        0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
        0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
        0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

    std::uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                          0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    unsigned char buf[64]{};
    std::size_t   len{0};       // bytes pendientes en `buf`
    std::uint64_t total{0};     // bytes consumidos en total

    static std::uint32_t rotr(std::uint32_t x, int n) {
        return (x >> n) | (x << (32 - n));
    }

    void compress(const unsigned char* p) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (std::uint32_t(p[4 * i]) << 24) | (std::uint32_t(p[4 * i + 1]) << 16)
                 | (std::uint32_t(p[4 * i + 2]) << 8) | std::uint32_t(p[4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18)
                                   ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19)
                                   ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3],
                      e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const unsigned char* p, std::size_t n) {
        total += n;
        while (n) {
            const std::size_t take = std::min(n, sizeof buf - len);
            std::memcpy(buf + len, p, take);
            len += take; p += take; n -= take;
            if (len == sizeof buf) { compress(buf); len = 0; }
        }
    }

    std::string hex() {
        const std::uint64_t bits = total * 8;      // antes de rellenar
        unsigned char byte = 0x80;
        update(&byte, 1);
        byte = 0x00;
        while (len != 56) update(&byte, 1);
        unsigned char tail[8];
        for (int i = 0; i < 8; ++i) tail[i] = (bits >> (56 - 8 * i)) & 0xff;
        update(tail, 8);
        static const char* dig = "0123456789abcdef";
        std::string out(64, '0');
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) {
                const unsigned char v = (h[i] >> (24 - 8 * j)) & 0xff;
                out[i * 8 + j * 2]     = dig[v >> 4];
                out[i * 8 + j * 2 + 1] = dig[v & 0xf];
            }
        return out;
    }
};

// Perfil XML de QoS realmente cargado por Fast DDS, si lo hay: {ruta, sha256}.
// Vacio significa que NO habia ninguno, y eso es lo normal aqui.
//
// El gateway no carga ningun perfil: el QoS se fija en codigo (D8) y en el
// repositorio no existe ningun XML. Pero Fast DDS carga uno solo con que
// aparezca, sin que nadie se lo pida --la variable de entorno
// `FASTRTPS_DEFAULT_PROFILES_FILE`, o un `DEFAULT_FASTDDS_PROFILES.xml` en el
// directorio de trabajo-- y ese es exactamente el fallo que estos dos campos
// existen para descartar: un XML olvidado cambiaria el QoS de una campana
// entera y ningun manifiesto lo diria.
//
// Por eso se miran los dos sitios. `null` en el manifiesto significa "se
// comprobo y no habia", no "no se miro", y esa diferencia es la unica razon
// por la que el campo vale algo.
std::pair<std::string, std::string> loaded_xml_profile() {
    namespace fs = std::filesystem;
    fs::path p;
    if (const char* env = std::getenv("FASTRTPS_DEFAULT_PROFILES_FILE"); env && *env)
        p = env;
    else if (fs::exists("DEFAULT_FASTDDS_PROFILES.xml"))
        p = "DEFAULT_FASTDDS_PROFILES.xml";
    else
        return {};

    std::error_code ec;
    const fs::path abs = fs::absolute(p, ec);
    const std::string shown = ec ? p.string() : abs.string();

    std::ifstream in(p, std::ios::binary);
    if (!in) {
        // Se pidio un perfil y no se puede leer. El hash vacio no pasa el
        // patron del esquema y la corrida se anula por `v4_incomplete`, que
        // es lo que toca: nadie puede decir con que QoS se midio.
        std::fprintf(stderr, "measurement_run: perfil XML ilegible: %s\n",
                     shown.c_str());
        return {shown, ""};
    }
    Sha256 sh;
    char chunk[16384];
    while (in.read(chunk, sizeof chunk) || in.gcount())
        sh.update(reinterpret_cast<const unsigned char*>(chunk),
                  static_cast<std::size_t>(in.gcount()));
    return {shown, sh.hex()};
}

}  // namespace

int main(int argc, char** argv) {
    if (!PublishProbe::enabled) {
        std::fprintf(stderr, "construido sin GATEWAY_INSTRUMENT: nada que medir\n");
        return 2;
    }

    Args a;
    for (int i = 1; i < argc; ++i) {
        auto need = [&]() -> std::string {
            if (i + 1 >= argc) { std::fprintf(stderr, "falta valor de %s\n", argv[i]); std::exit(2); }
            return argv[++i];
        };
        const std::string k = argv[i];
        if (k == "--typing")         a.typing = need();
        else if (k == "--formatter") a.formatter = need();
        else if (k == "--regime")    a.regime = need();
        else if (k == "--run-id")    a.run_id = need();
        else if (k == "--out")       a.out = need();
        else if (k == "--rate")      a.rate = std::stod(need());
        else if (k == "--window-s")  a.window_s = std::stod(need());
        else if (k == "--settle-s")  a.settle_s = std::stod(need());
        else if (k == "--fanout")    a.fanout = std::stoi(need());
        else if (k == "--domain")    a.domain = std::stoi(need());
        else if (k == "--port")      a.port = static_cast<std::uint16_t>(std::stoi(need()));
        else { std::fprintf(stderr, "argumento desconocido: %s\n", k.c_str()); return 2; }
    }

    const std::string sentence = sentence_for(a.formatter);
    if (sentence.empty()) {
        std::fprintf(stderr, "formatter sin sentencia de prueba: %s\n", a.formatter.c_str());
        return 2;
    }

    // Se comprueba ANTES de crear nada, no al escribir el manifiesto: si hay
    // un XML donde no deberia haberlo, el aviso sirve en el segundo cero y no
    // al final de una ventana de cinco minutos.
    const auto xml_profile = loaded_xml_profile();
    if (!xml_profile.first.empty())
        std::fprintf(stderr, "measurement_run: AVISO, Fast DDS cargara el perfil "
                             "XML %s; el QoS medido puede no ser el del codigo\n",
                     xml_profile.first.c_str());

    std::error_code ec;
    std::filesystem::create_directories(a.out, ec);

    const Registry reg = Registry::builtin();
    PublishProbe probe(20'000'000);

    // Suscriptores: el factor `fanout`. Viven en su propio participante para no
    // contarse como parte del proceso medido.
    auto* factory = DomainParticipantFactory::get_instance();
    auto* sub_part = factory->create_participant(a.domain, PARTICIPANT_QOS_DEFAULT);
    std::vector<DataReader*> readers;
    if (sub_part) {
        const Mapper m(reg);
        const auto info = m.resolve(a.formatter == "GGA" ? "GPGGA" : ("XX" + a.formatter));
        TypeSupport ts(new DynamicPubSubType(m.type_for(a.formatter)));
        sub_part->register_type(ts, info.type_name);
        auto* topic = sub_part->create_topic(info.topic_name, info.type_name, TOPIC_QOS_DEFAULT);
        auto* sub = sub_part->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        for (int i = 0; i < a.fanout && topic && sub; ++i)
            readers.push_back(sub->create_datareader(topic, DATAREADER_QOS_DEFAULT));
    }

    auto src = std::make_unique<UdpSource>();
    if (!src->open(a.port)) { std::fprintf(stderr, "no se pudo abrir UDP :%u\n", a.port); return 1; }

    Pipeline::Config cfg;
    cfg.typing = (a.typing == "static") ? Pipeline::Typing::Static : Pipeline::Typing::Dynamic;
    cfg.transports = Pipeline::Transports::Udpv4Only;   // explícitos y legibles de vuelta
    cfg.device_id = "pilot";
    cfg.source = std::move(src);
    cfg.registry = &reg;
    cfg.domain_id = a.domain;
    cfg.publish_to_dds = true;
    cfg.probe = &probe;

    Pipeline pipe(std::move(cfg));
    pipe.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    if (pipe.state() == Pipeline::State::Error) {
        std::fprintf(stderr, "el pipeline no arranco: %s\n", pipe.error_message().c_str());
        return 1;
    }

    // Generador en su propio hilo. Pinearlo aparte es trabajo del arnés
    // (`isolation.taskset_load` del manifiesto), no de aquí.
    std::atomic<bool> stop_gen{false};
    std::atomic<std::uint64_t> offered{0};
    std::thread gen([&] {
        const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_port = htons(a.port);
        to.sin_addr.s_addr = inet_addr("127.0.0.1");
        const auto period = (a.rate > 0.0)
                ? std::chrono::nanoseconds(static_cast<long long>(1e9 / a.rate))
                : std::chrono::nanoseconds(0);
        auto next = std::chrono::steady_clock::now();
        while (!stop_gen.load(std::memory_order_relaxed)) {
            ::sendto(fd, sentence.data(), sentence.size(), 0,
                     reinterpret_cast<sockaddr*>(&to), sizeof to);
            offered.fetch_add(1, std::memory_order_relaxed);
            if (period.count() > 0) {
                next += period;
                std::this_thread::sleep_until(next);
            }
        }
        ::close(fd);
    });

    // ── Asentamiento ────────────────────────────────────────────────────────
    std::this_thread::sleep_for(std::chrono::duration<double>(a.settle_s));
    const std::size_t warm_up_samples = probe.size();
    if (!probe.dump_csv(a.out + "/warmup.csv")) {   // archivadas, no descartadas
        std::cerr << "measurement_run: no se pudo escribir warmup.csv (disco lleno?)\n";
        stop_gen.store(true);
        gen.join();           // sin esto el destructor de un hilo joinable aborta
        pipe.stop();
        return 3;
    }

    // El orden de estas dos lineas fija el SIGNO del error de frontera, y el
    // signo importa. Una sentencia enviada justo antes de abrir y publicada
    // justo despues cae en la ventana por un lado y no por el otro; la carrera
    // es inherente y perseguirla no vale la pena, elegir hacia donde sesga si.
    //
    // Tomando offered ANTES de limpiar, esa sentencia cuenta como ofrecida y
    // no como publicada, asi que aparece como una descartada que no lo fue. Al
    // reves apareceria como publicada sin haberse ofrecido, que daria un
    // sentences_dropped negativo -- prohibido por el esquema -- y habria que
    // recortarlo a cero, escondiendo el efecto.
    //
    // El sesgo elegido es el conservador: H2 exige CERO descartadas para que un
    // probe pase, asi que una descartada de mas hace fallar un probe que quiza
    // paso, y nunca al reves.
    const std::uint64_t offered_at_open = offered.load();
    probe.clear();

    // ── Ventana de medición ─────────────────────────────────────────────────
    const auto t_open = std::chrono::system_clock::now();
    const double instr_p99 = PublishProbe::clock_pair_overhead_p99_ns();
    std::this_thread::sleep_for(std::chrono::duration<double>(a.window_s));
    const auto t_close = std::chrono::system_clock::now();

    const std::size_t measured = probe.size();
    const std::uint64_t dropped_by_probe = probe.dropped();
    const std::uint64_t offered_in_window = offered.load() - offered_at_open;

    stop_gen.store(true);
    gen.join();

    // Ruidoso a proposito: un CSV a medias y uno correcto son indistinguibles
    // aguas abajo, porque el checksum de un archivo truncado cuadra igual.
    if (!probe.dump_csv(a.out + "/latencies.csv")) {
        std::cerr << "measurement_run: no se pudo escribir latencies.csv con "
                  << measured << " muestras (disco lleno?)\n";
        pipe.stop();          // el generador ya se unio mas arriba
        return 4;
    }
    const auto transports = pipe.effective_transports();
    const auto discovery  = pipe.effective_discovery();
    const int  domain_id  = pipe.effective_domain_id();
    const bool shm = std::find(transports.begin(), transports.end(), "SHM")
                     != transports.end();
    pipe.stop();

    if (sub_part) {
        sub_part->delete_contained_entities();
        factory->delete_participant(sub_part);
    }

    // ── Manifiesto parcial: solo lo que este proceso sabe ───────────────────
    auto iso = [](std::chrono::system_clock::time_point tp) {
        const std::time_t t = std::chrono::system_clock::to_time_t(tp);
        char buf[32];
        std::strftime(buf, sizeof buf, "%FT%TZ", std::gmtime(&t));
        return std::string(buf);
    };

    std::ofstream o(a.out + "/manifest_partial.json");
    o << "{\n"
      << "  \"run_id\": \"" << json_escape(a.run_id) << "\",\n"
      << "  \"typing\": \"" << json_escape(a.typing) << "\",\n"
      << "  \"regime\": \"" << json_escape(a.regime) << "\",\n"
      << "  \"formatter\": \"" << json_escape(a.formatter) << "\",\n"
      << "  \"fanout\": " << a.fanout << ",\n"
      << "  \"timing\": {\n"
      << "    \"clock\": \"CLOCK_MONOTONIC_RAW\",\n"
      << "    \"start_wall_utc\": \"" << iso(t_open) << "\",\n"
      << "    \"end_wall_utc\": \"" << iso(t_close) << "\",\n"
      << "    \"settle_s\": " << a.settle_s << ",\n"
      << "    \"window_s\": " << a.window_s << ",\n"
      << "    \"instrument_p99_ns\": " << instr_p99 << "\n"
      << "  },\n"
      << "  \"counters\": {\n"
      << "    \"sentences_offered\": " << offered_in_window << ",\n"
      << "    \"sentences_published\": " << measured << ",\n"
      << "    \"sentences_dropped\": " << (offered_in_window > measured
                                           ? offered_in_window - measured : 0) << ",\n"
      << "    \"parse_failures\": " << pipe.sentences_err() << ",\n"
      << "    \"samples_recorded\": " << measured << "\n"
      << "  },\n"
      << "  \"dds\": {\n"
      << "    \"fastdds_version\": \"" << FASTDDS_VERSION_STR << "\",\n"
      << "    \"fastcdr_version\": \"" << FASTCDR_VERSION_STR << "\",\n"
      << "    \"xml_profile_path\": "
      << (xml_profile.first.empty() ? std::string("null")
                                    : "\"" + json_escape(xml_profile.first) + "\"") << ",\n"
      << "    \"xml_profile_sha256\": "
      << (xml_profile.first.empty() ? std::string("null")
                                    : "\"" + json_escape(xml_profile.second) + "\"") << ",\n"
      << "    \"transports\": [";
    for (std::size_t i = 0; i < transports.size(); ++i)
        o << (i ? ", " : "") << "\"" << json_escape(transports[i]) << "\"";
    o << "],\n"
      << "    \"shared_memory_enabled\": " << (shm ? "true" : "false") << ",\n"
      << "    \"discovery\": \"" << json_escape(discovery) << "\",\n"
      << "    \"domain_id\": " << domain_id << ",\n"
      << "    \"subscriber_count\": " << readers.size() << "\n  },\n"
      << "  \"probe\": {\n"
      << "    \"settling_samples_archived\": " << warm_up_samples << ",\n"
      << "    \"samples_dropped_by_probe\": " << dropped_by_probe << "\n"
      << "  }\n}\n";

    std::printf("%s  %s/%s/%s  ventana %.0fs  %zu muestras  ofrecidas %llu  "
                "descartadas-sonda %llu  transportes",
                a.run_id.c_str(), a.typing.c_str(), a.regime.c_str(), a.formatter.c_str(),
                a.window_s, measured, static_cast<unsigned long long>(offered_in_window),
                static_cast<unsigned long long>(dropped_by_probe));
    for (const auto& t : transports) std::printf(" %s", t.c_str());
    std::printf("\n");

    return measured > 0 ? 0 : 1;
}
