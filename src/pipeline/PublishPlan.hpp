#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "pipeline/Pipeline.hpp"  // Pipeline::QosSettings

namespace nmea {

// Allowlist de tramas convertidas (D: flujo Sensor→Trama→Tópico).
// Mapea (talker, formatter) → device_id (@key) + QoS del writer.
// Thread-safe: el hilo UI muta (add/remove); el hilo worker consulta (resolve,
// active_formatters). El sufijo de tópico/tipo lo deriva el Mapper, no esta clase.
class PublishPlan {
public:
    struct Target {
        std::string           device_id;
        Pipeline::QosSettings qos;
    };

    // Habilita la conversión de (talker, formatter). Reemplaza si ya existía.
    void add(const std::string& talker, const std::string& formatter,
             const std::string& device_id, const Pipeline::QosSettings& qos);

    // Deshabilita (talker, formatter). No-op si no estaba.
    void remove(const std::string& talker, const std::string& formatter);

    // ¿Está habilitada? Devuelve el Target si sí.
    std::optional<Target> resolve(const std::string& talker,
                                  const std::string& formatter) const;

    bool contains(const std::string& talker, const std::string& formatter) const;

    // Formatters con al menos una entrada activa (para reconciliar writers).
    std::vector<std::string> active_formatters() const;

private:
    static std::string key(const std::string& talker, const std::string& formatter) {
        return talker + '\x1f' + formatter;  // 0x1F = separador de unidad
    }
    mutable std::mutex mu_;
    std::unordered_map<std::string, Target> entries_;
};

}  // namespace nmea
