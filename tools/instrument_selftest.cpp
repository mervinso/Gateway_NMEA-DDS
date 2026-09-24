// tools/instrument_selftest.cpp — verifica la instrumentación de la ruta de
// publicación (obligación 4 del Cap. 8). Solo se construye con
// -DGATEWAY_INSTRUMENT=ON.
//
// Produce dos cosas que la campaña necesita y que no son opinables:
//
//  1. **`timing.instrument_p99_ns`** — el costo del par de relojes medido en
//     este host. La compuerta 2 de la metodología exige que quede por debajo de
//     500 ns en toda la campaña, y el manifiesto lo registra por corrida como
//     evidencia de que la compuerta sigue valiendo.
//  2. **Una comprobación de extremo a extremo** de que la sonda cuenta lo que
//     dice contar: N sentencias entran por UDP loopback, N muestras salen, con
//     latencias positivas y finitas.
//
// La segunda importa porque un instrumento que registra de menos no se anuncia:
// la corrida saldría con menos muestras de las que el manifiesto declara, y sin
// esta prueba el primer sitio donde se notaría sería el cruce contra
// `counters.samples_recorded` con la campaña ya corrida.

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture/UdpSource.hpp"
#include "instrument/PublishProbe.hpp"
#include "pipeline/Pipeline.hpp"
#include "registry/Registry.hpp"

using namespace nmea;

namespace {

constexpr std::uint16_t PORT = 34567;
constexpr int SENTENCES = 500;
// La compuerta 2 de la metodología, en nanosegundos.
constexpr double GATE2_MAX_P99_NS = 500.0;

int failures = 0;

void check(const char* name, bool ok, const std::string& detail) {
    std::printf("  [%s] %-46s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
    if (!ok) ++failures;
}

// Checksum NMEA: XOR de todo lo que va entre '$' y '*'.
std::string sentence(int i) {
    char body[96];
    std::snprintf(body, sizeof body, "SDDBT,%03d.%01d,f,%03d.%01d,M,%03d.%01d,F",
                  i % 1000, i % 10, i % 1000, i % 10, i % 1000, i % 10);
    unsigned ck = 0;
    for (const char* p = body; *p; ++p) ck ^= static_cast<unsigned char>(*p);
    char out[128];
    std::snprintf(out, sizeof out, "$%s*%02X\r\n", body, ck);
    return out;
}

}  // namespace

int main() {
    std::printf("instrument_selftest: instrumentacion de la ruta de publicacion\n\n");

    if (!PublishProbe::enabled) {
        std::printf("  construido sin GATEWAY_INSTRUMENT; nada que verificar\n");
        return 2;
    }

    // ── 1. Costo del par de relojes ─────────────────────────────────────────
    const double p99 = PublishProbe::clock_pair_overhead_p99_ns();
    check("el costo del par de relojes pasa la compuerta 2",
          p99 < GATE2_MAX_P99_NS,
          "p99 = " + std::to_string(p99) + " ns, limite " +
                  std::to_string(static_cast<int>(GATE2_MAX_P99_NS)) + " ns");

    // El reloj debe avanzar de verdad: un p99 de 0 significaria una fuente de
    // tiempo con granularidad mayor que el intervalo que medimos.
    check("el reloj tiene resolucion suficiente para medir el par",
          p99 > 0.0, "un p99 de cero significaria granularidad insuficiente");

    // ── 2. Extremo a extremo por UDP loopback ───────────────────────────────
    const Registry reg = Registry::builtin();
    PublishProbe probe(SENTENCES * 4);

    auto src = std::make_unique<UdpSource>();
    if (!src->open(PORT)) {
        std::printf("  no se pudo abrir UDP :%u\n", PORT);
        return 1;
    }

    Pipeline::Config cfg;
    cfg.device_id = "selftest";
    cfg.source = std::move(src);
    cfg.registry = &reg;
    cfg.domain_id = 198;          // dominio propio, para no tropezar con nada
    cfg.publish_to_dds = true;
    cfg.probe = &probe;

    Pipeline pipe(std::move(cfg));
    pipe.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));   // que DDS arranque

    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(PORT);
    to.sin_addr.s_addr = inet_addr("127.0.0.1");
    for (int i = 0; i < SENTENCES; ++i) {
        const std::string s = sentence(i);
        ::sendto(fd, s.data(), s.size(), 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    ::close(fd);
    pipe.stop();

    check("la sonda registro una muestra por sentencia publicada",
          probe.size() == static_cast<std::size_t>(SENTENCES),
          std::to_string(probe.size()) + " muestras de " + std::to_string(SENTENCES) +
                  " sentencias enviadas");
    check("no se descarto ninguna muestra por falta de reserva",
          probe.dropped() == 0,
          std::to_string(probe.dropped()) + " descartadas");

    bool positive = true, labelled = true;
    std::int64_t lo = 0, hi = 0, sum = 0;
    for (const auto& s : probe.samples()) {
        if (s.publish_path_ns <= 0) positive = false;
        if (std::strcmp(s.formatter, "DBT") != 0) labelled = false;
        if (lo == 0 || s.publish_path_ns < lo) lo = s.publish_path_ns;
        if (s.publish_path_ns > hi) hi = s.publish_path_ns;
        sum += s.publish_path_ns;
    }
    const double mean = probe.size() ? double(sum) / double(probe.size()) : 0.0;

    check("toda latencia medida es estrictamente positiva", positive,
          "min " + std::to_string(lo) + " ns, media " + std::to_string(mean) +
                  " ns, max " + std::to_string(hi) + " ns");
    check("cada muestra lleva su formatter", labelled,
          "las 500 sentencias eran SDDBT, formatter DBT");
    check("la latencia medida supera holgadamente el suelo del instrumento",
          mean > 10.0 * p99,
          "media " + std::to_string(mean) + " ns contra un suelo p99 de " +
                  std::to_string(p99) + " ns");

    // Las seq deben ser consecutivas desde 0: un hueco significaria muestras
    // perdidas sin que dropped() lo dijera.
    bool consecutive = true;
    for (std::size_t i = 0; i < probe.samples().size(); ++i)
        if (probe.samples()[i].seq != i) { consecutive = false; break; }
    check("las secuencias son consecutivas desde cero", consecutive,
          "un hueco seria perdida silenciosa de muestras");

    const std::string csv = "/tmp/instrument_selftest.csv";
    check("el volcado a CSV funciona", probe.dump_csv(csv), csv);

    std::printf("\n%d verificacion(es) fallida(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
