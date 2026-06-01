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

// Una conversión NMEA→DDS en ejecución (D9/D13).
// Cada instancia es un hilo POSIX independiente:
//   read(source) → Parser FSM → Mapper → DynamicData → DataWriter
class Pipeline {
public:
    enum class State { Stopped, Running, Error };

    struct Config {
        std::string              device_id;    // @key DDS (D3)
        std::unique_ptr<ISource> source;       // propiedad exclusiva (D9)
        const Registry*          registry;     // no owned, debe sobrevivir al pipeline
        int                      domain_id{0};

        // Si false, el worker parsea y cuenta pero no crea DataWriters ni publica.
        // Usado por GatewayController en modo preview (detección sin DDS).
        bool publish_to_dds{true};

        // Callback opcional invocado desde el hilo worker por cada sentencia válida.
        // Los strings son copias seguras (no string_view). Puede ser nullptr.
        std::function<void(std::string formatter,
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
