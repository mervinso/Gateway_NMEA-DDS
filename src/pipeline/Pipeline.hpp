#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture/ISource.hpp"
#include "registry/Registry.hpp"

namespace nmea {

class PublishPlan;  // definido en PublishPlan.hpp; el Config solo guarda un puntero

// Una conversión NMEA→DDS en ejecución (D9/D13).
// Cada instancia es un hilo POSIX independiente:
//   read(source) → Parser FSM → Mapper → DynamicData → DataWriter
class Pipeline {
public:
    enum class State { Stopped, Running, Error };

    // Ajustes QoS neutrales (sin dependencia de Fast DDS ni de la UI) que el
    // worker traduce a DataWriterQos al crear cada DataWriter (D8).
    // deadline_ms / lifespan_ms == 0 ⇒ infinito (no se fija la política).
    struct QosSettings {
        bool reliable{false};         // false=BEST_EFFORT, true=RELIABLE
        bool transient_local{false};  // false=VOLATILE, true=TRANSIENT_LOCAL (keep_last 1)
        int  deadline_ms{0};
        int  lifespan_ms{0};
    };

    struct Config {
        std::string              device_id;    // @key DDS (D3)
        std::unique_ptr<ISource> source;       // propiedad exclusiva (D9)
        const Registry*          registry;     // no owned, debe sobrevivir al pipeline
        int                      domain_id{0};

        // Si false, el worker parsea y cuenta pero no crea DataWriters ni publica.
        // Usado por GatewayController en modo preview (detección sin DDS).
        bool publish_to_dds{true};

        // Perfil QoS aplicado a cada DataWriter creado (D8).
        QosSettings qos{};

        // Allowlist de tramas a publicar (no owned). Si es nullptr, el Pipeline
        // publica TODAS las sentencias con device_id/qos globales (modo legacy).
        // Si != nullptr, publica solo lo habilitado, con la clave de cada sensor.
        PublishPlan* plan{nullptr};

        // Callback opcional invocado desde el hilo worker por cada sentencia válida.
        // Los strings son copias seguras (no string_view). Puede ser nullptr.
        std::function<void(std::string talker,
                           std::string formatter,
                           std::string category,
                           std::vector<std::string> field_names,
                           std::vector<std::string> field_values)> on_sentence;
    };

    explicit Pipeline(Config cfg);
    ~Pipeline();

    // Lanza el hilo trabajador. No-op si ya está en Running.
    void start();

    // Señala parada y bloquea hasta que el hilo termine. No-op si Stopped/Error.
    void stop();

    State       state()         const noexcept;
    std::string error_message() const;

    uint64_t sentences_ok()  const noexcept { return ok_.load(std::memory_order_relaxed); }
    uint64_t sentences_err() const noexcept { return err_.load(std::memory_order_relaxed); }

private:
    void worker_loop();

    Config                cfg_;
    std::atomic<State>    state_{State::Stopped};
    std::atomic<bool>     stop_{false};
    std::thread           thread_;
    std::atomic<uint64_t> ok_{0};
    std::atomic<uint64_t> err_{0};
    std::string           error_msg_;
};

}  // namespace nmea
