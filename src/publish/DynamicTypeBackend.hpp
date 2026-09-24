// src/publish/DynamicTypeBackend.hpp — el brazo bajo prueba.
//
// Construye el tipo en ejecución con XTypes y puebla un `DynamicData` miembro a
// miembro, buscando cada identificador **por nombre**. Ese es el mecanismo que
// H4 pone a prueba: predice que el sobrecosto crece con el número de miembros y
// no con la tasa de sentencias.
//
// Es el comportamiento que el gateway ya tenía; aquí solo queda detrás del seam
// para que el brazo generado pueda ponerse al lado sin duplicar la gestión de
// writers.

#pragma once

#include <string>
#include <unordered_map>

#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>

#include "publish/ITypeBackend.hpp"

namespace nmea {

class DynamicTypeBackend final : public ITypeBackend {
public:
    explicit DynamicTypeBackend(const Mapper& mapper) : mapper_(mapper) {}

    const char* arm_name() const override { return "dynamic"; }

    bool prepare(const Mapper::SentenceInfo& info,
                 eprosima::fastdds::dds::TypeSupport& out) override {
        const auto type = info.formatter.empty() ? mapper_.raw_sentence_type()
                                                 : mapper_.type_for(info.formatter);
        if (!type) return false;
        // El tipo se retiene aquí y no en la entrada del writer: quién mantiene
        // vivo el `DynamicType` es un detalle de este brazo, y el gestor de
        // writers es compartido.
        types_[key(info)] = type;
        out = eprosima::fastdds::dds::TypeSupport(
                new eprosima::fastdds::dds::DynamicPubSubType(type));
        return true;
    }

    bool write(eprosima::fastdds::dds::DataWriter* w,
               const Mapper::SentenceInfo& info,
               const SentenceView& view,
               std::string_view device_id,
               std::int64_t recv_ns) override {
        auto it = types_.find(key(info));
        if (it == types_.end()) return false;

        // Asignación en heap por sentencia. §8.4 la nombra como uno de los
        // costos de este brazo, frente a la estructura en pila del generado;
        // no es un descuido que se pueda quitar sin dejar de ser este brazo.
        auto data = eprosima::fastdds::dds::DynamicDataFactory::get_instance()
                            ->create_data(it->second);
        if (!data) return false;
        mapper_.populate(data, view, device_id, info.formatter, recv_ns);
        return w->write(&data) == eprosima::fastdds::dds::RETCODE_OK;
    }

private:
    static std::string key(const Mapper::SentenceInfo& info) {
        return info.formatter.empty() ? "raw" : info.formatter;
    }

    const Mapper& mapper_;
    std::unordered_map<std::string, eprosima::fastdds::dds::DynamicType::_ref_type> types_;
};

}  // namespace nmea
