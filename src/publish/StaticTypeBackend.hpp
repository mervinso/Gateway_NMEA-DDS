// src/publish/StaticTypeBackend.hpp — el brazo de comparación.
//
// Publica con tipos generados por `fastddsgen` a partir del IDL que
// `tools/idl_from_registry` emite desde el registro. El miembro se resuelve en
// compilación a un desplazamiento fijo, frente a la búsqueda por nombre y el
// despacho virtual por miembro del brazo dinámico: ese es el contraste que RQ1
// estima y el mecanismo que H4 pone a prueba.
//
// Casi no hay código aquí porque el trabajo está en el despacho generado. Esta
// clase solo cachea una instancia por formatter, para que la fábrica corra una
// vez por tipo y no una vez por sentencia.
//
// Solo existe cuando se construye con `-DGATEWAY_STATIC_ARM=ON`, porque
// necesita el código que `fastddsgen` produce.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "NmeaStaticDispatch.hpp"
#include "publish/ITypeBackend.hpp"

namespace nmea {

class StaticTypeBackend final : public ITypeBackend {
public:
    const char* arm_name() const override { return "static"; }

    bool prepare(const Mapper::SentenceInfo& info,
                 eprosima::fastdds::dds::TypeSupport& out) override {
        IStaticType* t = get(info.formatter);
        if (!t) return false;
        out = t->type_support();
        return true;
    }

    bool write(eprosima::fastdds::dds::DataWriter* w,
               const Mapper::SentenceInfo& info,
               const SentenceView& view,
               std::string_view device_id,
               std::int64_t recv_ns) override {
        IStaticType* t = get(info.formatter);
        if (!t) return false;
        return t->write(w, view, device_id, info.talker, recv_ns);
    }

private:
    // Devuelve nullptr para un formatter sin tipo generado —RawSentence entre
    // ellos—. El brazo generado cubre los de la campaña y nada más, y decirlo
    // es mejor que publicar algo distinto del otro brazo: la sentencia no
    // publicada se ve como un hueco en los contadores, no como un dato bueno.
    IStaticType* get(const std::string& formatter) {
        auto it = types_.find(formatter);
        if (it != types_.end()) return it->second.get();
        auto made = make_static_type(formatter);
        if (!made) return nullptr;
        IStaticType* raw = made.get();
        types_.emplace(formatter, std::move(made));
        return raw;
    }

    std::unordered_map<std::string, std::unique_ptr<IStaticType>> types_;
};

}  // namespace nmea
