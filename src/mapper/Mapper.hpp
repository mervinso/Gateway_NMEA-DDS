#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>

#include "parser/Parser.hpp"
#include "registry/Registry.hpp"

namespace nmea {

// Convierte un SentenceView parseado en un DynamicData de Fast DDS (D5/D6).
//
// Responsabilidades:
//   - Resolver el formatter desde el address del SentenceView (talker+formatter → formatter).
//   - Construir y cachear los DynamicType a partir del Registry (una vez por formatter).
//   - Poblar un DynamicData con device_id (@key), talker, recv_timestamp y los campos del sensor.
//   - Formatters desconocidos → tipo RawSentence (D5).
class Mapper {
public:
    explicit Mapper(const Registry& registry);

    // Devuelve un DynamicData listo para publicar.
    // device_id es el @key de la instancia DDS (D3).
    // recv_ns es el timestamp de recepción en nanosegundos epoch (0 = usar reloj del sistema).
    eprosima::fastdds::dds::DynamicData::_ref_type map(
            const SentenceView& view,
            std::string_view device_id,
            int64_t recv_ns = 0) const;

    // Devuelve el DynamicType para un formatter dado (para crear el DataWriter).
    // Usa el cache; construye si aún no existe.
    eprosima::fastdds::dds::DynamicType::_ref_type type_for(
            std::string_view formatter) const;

    // DynamicType del tipo genérico RawSentence.
    eprosima::fastdds::dds::DynamicType::_ref_type raw_sentence_type() const;

    // Puebla un DynamicData ya creado con los valores de la sentencia + device_id.
    // Útil cuando el DynamicData fue creado externamente (Pipeline) para controlar
    // el ciclo de vida del DynamicType. recv_ns = 0 usa el reloj del sistema.
    //
    // Esta forma resuelve el formatter a partir del address. El Pipeline ya lo
    // tiene resuelto cuando llega aquí, así que usa la sobrecarga de abajo: el
    // formatter se resolvía dos veces por sentencia, y esa segunda resolución
    // cae dentro de la región cronometrada (retorno del parser → retorno de
    // write). Ver tesis §8.6.1.
    void populate(eprosima::fastdds::dds::DynamicData::_ref_type& data,
                  const SentenceView& view,
                  std::string_view device_id,
                  int64_t recv_ns = 0) const;

    // Igual que la anterior, con el formatter ya resuelto por el llamante.
    // `formatter` vacío significa "desconocido" → se puebla como RawSentence,
    // exactamente el mismo significado que devuelve resolve_formatter().
    void populate(eprosima::fastdds::dds::DynamicData::_ref_type& data,
                  const SentenceView& view,
                  std::string_view device_id,
                  std::string_view formatter,
                  int64_t recv_ns) const;

    // Metadatos de una sentencia por su address ("GPGGA", "VNYMR", ...).
    // Usados por el Pipeline para nombrar tópicos y tipos DDS.
    struct SentenceInfo {
        std::string formatter;    // "GGA", "VNYMR", "" si desconocido
        std::string talker;       // "GP", "" si propietario o desconocido
        std::string topic_name;   // "nmea/gps/GGA", "nmea/raw/RawSentence"
        std::string type_name;    // "NmeaGGA", "RawSentence"
    };
    SentenceInfo resolve(std::string_view address) const;

private:
    // Extrae el formatter de un address: "GPGGA"→"GGA", "VNYMR"→"VNYMR".
    std::string resolve_formatter(std::string_view address) const;

    eprosima::fastdds::dds::DynamicType::_ref_type build_type(
            const SentenceDef& def) const;

    eprosima::fastdds::dds::DynamicType::_ref_type build_raw_sentence_type() const;

    const Registry& registry_;
    mutable std::unordered_map<std::string,
            eprosima::fastdds::dds::DynamicType::_ref_type> type_cache_;
    mutable eprosima::fastdds::dds::DynamicType::_ref_type raw_type_;
};

}  // namespace nmea
