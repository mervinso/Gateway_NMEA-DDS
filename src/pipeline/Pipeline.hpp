#pragma once

#include <mutex>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture/ISource.hpp"
#include "instrument/PublishProbe.hpp"
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

    // El factor `typing` del experimento de RQ1. El brazo generado solo existe
    // si se construyó con -DGATEWAY_STATIC_ARM=ON; pedirlo sin eso es un error
    // de configuración y el Pipeline lo dice en vez de caer al otro brazo en
    // silencio, que produciría una corrida entera etiquetada como el brazo
    // equivocado.
    enum class Typing { Dynamic, Static };

    // Política de transportes.
    //
    // `Builtin` deja los que Fast DDS elija, que es el comportamiento de
    // siempre y el que la operación normal quiere.
    //
    // `Udpv4Only` los configura **explícitamente** en el QoS del participante.
    // El diseño de medición exige UDPv4 sin memoria compartida, y `v3_transport`
    // es criterio de anulación de corrida — pero la variable de entorno
    // `FASTDDS_BUILTIN_TRANSPORTS` **no se refleja en el QoS**: leerlo de vuelta
    // devuelve `use_builtin_transports = true` y cero transportes de usuario,
    // se haya exportado o no. Medido, no supuesto.
    //
    // Eso importa mas de lo que parece. Con la variable, un olvido produce una
    // corrida con memoria compartida —anulable por v3— y el manifiesto diria
    // "UDPv4" igual, porque nadie puede comprobarlo. Configurandolos aqui, el
    // artefacto los controla y `effective_transports()` los lee de vuelta para
    // que el manifiesto registre lo que paso y no lo que se pidio.
    enum class Transports { Builtin, Udpv4Only };

    struct Config {
        Typing typing{Typing::Dynamic};
        Transports transports{Transports::Builtin};

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

        // Sonda de instrumentación (no owned, puede ser nullptr). Con la opción
        // de build GATEWAY_INSTRUMENT apagada la clase es vacía y todas sus
        // llamadas desaparecen, así que este puntero no cuesta nada: el binario
        // medido es el binario publicado salvo un flag (Cap. 8, obligación 4).
        PublishProbe* probe{nullptr};

        // Callback opcional invocado desde el hilo worker por cada sentencia válida.
        // Los strings son copias seguras (no string_view). Puede ser nullptr.
        std::function<void(std::string talker,
                           std::string formatter,
                           std::string category,
                           std::vector<std::string> field_names,
                           std::vector<std::string> field_values)> on_sentence;
    };

    // Transportes efectivos del participante, leidos de su QoS despues de
    // crearlo. Vacio hasta que el hilo trabajador ha creado el participante, y
    // vacio siempre si `publish_to_dds` es false. Lo consume el emisor del
    // manifiesto de corrida (obligacion 4b).
    std::vector<std::string> effective_transports() const;

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
    // Escrito una sola vez por el hilo trabajador antes de senalar Running,
    // y solo leido despues. El mutex es barato y ocurre una vez por corrida.
    mutable std::mutex           transports_mtx_;
    std::vector<std::string>     transports_;
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
