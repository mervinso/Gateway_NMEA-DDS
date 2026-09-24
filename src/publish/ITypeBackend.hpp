// src/publish/ITypeBackend.hpp — el seam de RQ1.
//
// Los dos brazos del experimento difieren en exactamente dos cosas: de dónde
// sale el `TypeSupport` de un formatter, y cómo se convierte una sentencia en
// una muestra escrita. Todo lo demás —el participante, el publisher, la
// traducción de QoS, la caché de writers, la detección de cambio de QoS, la
// reconciliación y el cleanup— es idéntico y se comparte.
//
// Se factoriza así y no en dos gestores de writers paralelos porque duplicar
// esa lógica es como los dos brazos acaban difiriendo en algo que no es el
// tipado, y entonces RQ1 mide una diferencia de implementación en vez de la
// estrategia de tipos. La compuerta de equivalencia existe para atrapar eso;
// no dar ocasión es mejor que atraparlo.
//
// Los nombres de tópico y de tipo NO salen de aquí: salen de `Mapper::resolve`
// en los dos brazos, así que son idénticos por construcción y no por acuerdo.
//
// El coste del despacho virtual de `write` lo pagan los dos brazos por igual y
// se cancela en el contraste que RQ1 estima, igual que los costes de §8.6.1.
// Está dentro de la región cronometrada, que es donde tiene que estar: es parte
// de lo que cuesta publicar.

#pragma once

#include <cstdint>
#include <string_view>

#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include "mapper/Mapper.hpp"
#include "parser/Parser.hpp"

namespace nmea {

class ITypeBackend {
public:
    virtual ~ITypeBackend() = default;

    // Nombre del brazo, tal y como el manifiesto de corrida lo registra.
    virtual const char* arm_name() const = 0;

    // `TypeSupport` para registrar el tipo de este formatter. Devuelve false si
    // el brazo no puede publicar ese formatter — el brazo generado solo cubre
    // los de la campaña, y callarlo convertiría una sentencia no publicada en
    // un hueco silencioso en los datos.
    virtual bool prepare(const Mapper::SentenceInfo& info,
                         eprosima::fastdds::dds::TypeSupport& out) = 0;

    // Convierte la sentencia en una muestra y la escribe. Devuelve false si el
    // write falló.
    virtual bool write(eprosima::fastdds::dds::DataWriter* w,
                       const Mapper::SentenceInfo& info,
                       const SentenceView& view,
                       std::string_view device_id,
                       std::int64_t recv_ns) = 0;
};

}  // namespace nmea
