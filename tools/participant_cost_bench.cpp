// tools/participant_cost_bench.cpp — microbenchmark del costo de descubrimiento
// por participante (Tarea 4 del estudio de escalabilidad RQ3 de la tesis).
//
// Mide el CPU de proceso en estado estacionario de P DomainParticipants ociosos
// en un mismo dominio — descubrimiento SIMPLE, sin publicar ni suscribir nada —
// después de que el descubrimiento mutuo ha convergido. El término del modelo es
// D = d·P(P−1)/2 CPU-segundos por segundo; este arnés produce los puntos
// (P, cpu/s) a los que se ajusta d por OLS.
//
// Los participantes se crean con PARTICIPANT_QOS_DEFAULT, exactamente como lo
// hace Pipeline::worker_loop(), para que lo medido sea la configuración que el
// gateway despliega. El transporte se fija desde fuera (el barrido exporta
// FASTDDS_BUILTIN_TRANSPORTS=UDPv4), igual que en la campaña de medición.
//
// Con --participants 0 mide el mismo proceso sin ningún participante: es la
// línea base del paso 2 de la Tarea 4 — CPU(P=1) debe quedar dentro del ruido
// de CPU(P=0), o el arnés se está midiendo a sí mismo.
//
// Salida: una línea JSON en stdout por ejecución. El barrido, el orden
// aleatorizado y la procedencia los maneja tools/participant_cost_sweep.py.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <thread>
#include <vector>

#include <sys/resource.h>
#include <unistd.h>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>

using namespace eprosima::fastdds::dds;

// La convergencia se detecta con el listener, no con
// DomainParticipant::get_discovered_participants(): en Fast DDS 3.6.2 esa
// consulta devuelve una lista vacía aunque el descubrimiento sí haya ocurrido
// (verificado 2026-09-22 con dos participantes en un proceso: el callback
// disparó en ambos y la consulta siguió en cero). El listener es la vía
// canónica y es la que el arnés usa.
//
// El callback solo se ejecuta durante la fase de descubrimiento, que termina
// antes de que empiece la ventana de medición, así que no contamina el CPU
// medido.
struct DiscoveryCounter : DomainParticipantListener {
    std::atomic<int> peers{0};
    void on_participant_discovery(DomainParticipant*,
                                  eprosima::fastdds::rtps::ParticipantDiscoveryStatus status,
                                  const eprosima::fastdds::rtps::ParticipantBuiltinTopicData&,
                                  bool&) override {
        using S = eprosima::fastdds::rtps::ParticipantDiscoveryStatus;
        if (status == S::DISCOVERED_PARTICIPANT) {
            peers.fetch_add(1, std::memory_order_relaxed);
        } else if (status == S::REMOVED_PARTICIPANT || status == S::DROPPED_PARTICIPANT) {
            peers.fetch_sub(1, std::memory_order_relaxed);
        }
    }
};

static double now_monotonic_raw() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}

// CPU total del proceso (usuario + sistema), todos los hilos incluidos.
static double cpu_seconds(double* user, double* sys) {
    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    const double u = double(ru.ru_utime.tv_sec) + double(ru.ru_utime.tv_usec) * 1e-6;
    const double s = double(ru.ru_stime.tv_sec) + double(ru.ru_stime.tv_usec) * 1e-6;
    if (user) *user = u;
    if (sys) *sys = s;
    return u + s;
}

int main(int argc, char** argv) {
    int participants = -1;
    double window_s = 60.0;
    double settle_s = 10.0;
    double discovery_timeout_s = 120.0;
    int domain = 199;

    for (int i = 1; i < argc; ++i) {
        auto need = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "falta el valor de %s\n", argv[i]);
                std::exit(2);
            }
            return argv[++i];
        };
        if (!std::strcmp(argv[i], "--participants")) participants = std::atoi(need());
        else if (!std::strcmp(argv[i], "--window-s")) window_s = std::atof(need());
        else if (!std::strcmp(argv[i], "--settle-s")) settle_s = std::atof(need());
        else if (!std::strcmp(argv[i], "--discovery-timeout-s")) discovery_timeout_s = std::atof(need());
        else if (!std::strcmp(argv[i], "--domain")) domain = std::atoi(need());
        else {
            std::fprintf(stderr, "argumento desconocido: %s\n", argv[i]);
            return 2;
        }
    }
    if (participants < 0 || window_s <= 0.0) {
        std::fprintf(stderr,
                     "uso: participant_cost_bench --participants P [--window-s 60] "
                     "[--settle-s 10] [--discovery-timeout-s 120] [--domain 199]\n");
        return 2;
    }

    auto* factory = DomainParticipantFactory::get_instance();
    std::vector<DomainParticipant*> ps;
    std::vector<std::unique_ptr<DiscoveryCounter>> counters;
    ps.reserve(size_t(participants));
    counters.reserve(size_t(participants));

    for (int k = 0; k < participants; ++k) {
        counters.push_back(std::make_unique<DiscoveryCounter>());
        DomainParticipant* p =
                factory->create_participant(domain, PARTICIPANT_QOS_DEFAULT, counters.back().get());
        if (!p) {
            std::fprintf(stderr, "fallo create_participant (%d de %d)\n", k + 1, participants);
            return 1;
        }
        ps.push_back(p);
    }

    // Convergencia: cada participante debe haber descubierto a los otros P−1.
    // La ventana mide el régimen estacionario (anuncios periódicos, liveliness),
    // no la ráfaga SPDP/SEDP del arranque.
    double discovery_wait_s = 0.0;
    if (participants > 1) {
        const double t0 = now_monotonic_raw();
        for (;;) {
            bool converged = true;
            for (auto& c : counters) {
                if (c->peers.load(std::memory_order_relaxed) < participants - 1) {
                    converged = false;
                    break;
                }
            }
            discovery_wait_s = now_monotonic_raw() - t0;
            if (converged) break;
            if (discovery_wait_s > discovery_timeout_s) {
                std::fprintf(stderr, "descubrimiento no convergió en %.0f s (P=%d)\n",
                             discovery_timeout_s, participants);
                return 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(settle_s));

    // Ventana: el hilo principal duerme, así que todo el CPU acumulado es de los
    // hilos de Fast DDS. Reloj CLOCK_MONOTONIC_RAW, el mismo de la instrumentación.
    double u0 = 0, s0 = 0, u1 = 0, s1 = 0;
    const double cpu0 = cpu_seconds(&u0, &s0);
    const double w0 = now_monotonic_raw();
    std::this_thread::sleep_for(std::chrono::duration<double>(window_s));
    const double cpu1 = cpu_seconds(&u1, &s1);
    const double w1 = now_monotonic_raw();

    const double wall = w1 - w0;
    const double cpu = cpu1 - cpu0;

    char host[256] = "?";
    gethostname(host, sizeof host);

    std::printf("{\"participants\":%d,\"domain\":%d,\"window_s\":%.6f,"
                "\"cpu_s\":%.6f,\"cpu_user_s\":%.6f,\"cpu_sys_s\":%.6f,"
                "\"cpu_per_s\":%.9f,\"settle_s\":%.3f,\"discovery_wait_s\":%.3f,"
                "\"nproc\":%ld,\"hostname\":\"%s\"}\n",
                participants, domain, wall, cpu, u1 - u0, s1 - s0, cpu / wall,
                settle_s, discovery_wait_s, sysconf(_SC_NPROCESSORS_ONLN), host);

    for (auto* p : ps) factory->delete_participant(p);
    return 0;
}
