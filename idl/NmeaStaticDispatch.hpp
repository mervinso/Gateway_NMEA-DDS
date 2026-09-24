// GENERADO POR tools/idl_from_registry -- NO EDITAR A MANO.
//
// Despacho del brazo generado. Cada tipo de fastddsgen es una clase C++
// distinta, asi que el Pipeline no puede tratarlos uniformemente sin borrar el
// tipo. Esto emite una clase por formatter detras de una interfaz comun, y una
// fabrica que devuelve la que toque.
//
// El backend estatico cachea una instancia por formatter, asi que la fabrica se
// llama una vez por tipo y no una vez por sentencia.
//
// La muestra se declara **en la pila**. Es el punto del brazo generado: §8.4
// describe la asignacion en heap por sentencia como uno de los costos del brazo
// dinamico, y un tipo generado de IDL da una estructura que el compilador puede
// colocar en la pila.

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include "parser/Parser.hpp"

#include "NmeaStaticPopulate.hpp"
#include "NmeaROTPubSubTypes.hpp"
#include "NmeaRSAPubSubTypes.hpp"
#include "NmeaDBTPubSubTypes.hpp"
#include "NmeaVHWPubSubTypes.hpp"
#include "NmeaGGAPubSubTypes.hpp"

namespace nmea {

// Interfaz con el tipo borrado. Una implementacion por formatter, generada.
class IStaticType {
public:
    virtual ~IStaticType() = default;

    // TypeSupport del tipo generado, para register_type().
    virtual eprosima::fastdds::dds::TypeSupport type_support() const = 0;

    // Puebla una muestra en la pila y la escribe. Devuelve false si write fallo.
    virtual bool write(eprosima::fastdds::dds::DataWriter* w,
                       const SentenceView& v,
                       std::string_view device_id,
                       std::string_view talker,
                       std::int64_t recv_ns) const = 0;
};

class StaticTypeROT final : public IStaticType {
public:
    eprosima::fastdds::dds::TypeSupport type_support() const override {
        return eprosima::fastdds::dds::TypeSupport(new NmeaROTPubSubType());
    }
    bool write(eprosima::fastdds::dds::DataWriter* w,
               const SentenceView& v,
               std::string_view device_id,
               std::string_view talker,
               std::int64_t recv_ns) const override {
        NmeaROT s;   // en la pila, no en el heap
        populate_static(s, v, device_id, talker, recv_ns);
        return w->write(&s) == eprosima::fastdds::dds::RETCODE_OK;
    }
};

class StaticTypeRSA final : public IStaticType {
public:
    eprosima::fastdds::dds::TypeSupport type_support() const override {
        return eprosima::fastdds::dds::TypeSupport(new NmeaRSAPubSubType());
    }
    bool write(eprosima::fastdds::dds::DataWriter* w,
               const SentenceView& v,
               std::string_view device_id,
               std::string_view talker,
               std::int64_t recv_ns) const override {
        NmeaRSA s;   // en la pila, no en el heap
        populate_static(s, v, device_id, talker, recv_ns);
        return w->write(&s) == eprosima::fastdds::dds::RETCODE_OK;
    }
};

class StaticTypeDBT final : public IStaticType {
public:
    eprosima::fastdds::dds::TypeSupport type_support() const override {
        return eprosima::fastdds::dds::TypeSupport(new NmeaDBTPubSubType());
    }
    bool write(eprosima::fastdds::dds::DataWriter* w,
               const SentenceView& v,
               std::string_view device_id,
               std::string_view talker,
               std::int64_t recv_ns) const override {
        NmeaDBT s;   // en la pila, no en el heap
        populate_static(s, v, device_id, talker, recv_ns);
        return w->write(&s) == eprosima::fastdds::dds::RETCODE_OK;
    }
};

class StaticTypeVHW final : public IStaticType {
public:
    eprosima::fastdds::dds::TypeSupport type_support() const override {
        return eprosima::fastdds::dds::TypeSupport(new NmeaVHWPubSubType());
    }
    bool write(eprosima::fastdds::dds::DataWriter* w,
               const SentenceView& v,
               std::string_view device_id,
               std::string_view talker,
               std::int64_t recv_ns) const override {
        NmeaVHW s;   // en la pila, no en el heap
        populate_static(s, v, device_id, talker, recv_ns);
        return w->write(&s) == eprosima::fastdds::dds::RETCODE_OK;
    }
};

class StaticTypeGGA final : public IStaticType {
public:
    eprosima::fastdds::dds::TypeSupport type_support() const override {
        return eprosima::fastdds::dds::TypeSupport(new NmeaGGAPubSubType());
    }
    bool write(eprosima::fastdds::dds::DataWriter* w,
               const SentenceView& v,
               std::string_view device_id,
               std::string_view talker,
               std::int64_t recv_ns) const override {
        NmeaGGA s;   // en la pila, no en el heap
        populate_static(s, v, device_id, talker, recv_ns);
        return w->write(&s) == eprosima::fastdds::dds::RETCODE_OK;
    }
};

// Devuelve nullptr si el formatter no tiene tipo generado. El brazo
// estatico solo cubre los formatters de la campana; el resto no es
// publicable por esta via y el llamante debe decirlo, no callarlo.
inline std::unique_ptr<IStaticType> make_static_type(std::string_view formatter) {
    if (formatter == "ROT") return std::make_unique<StaticTypeROT>();
    if (formatter == "RSA") return std::make_unique<StaticTypeRSA>();
    if (formatter == "DBT") return std::make_unique<StaticTypeDBT>();
    if (formatter == "VHW") return std::make_unique<StaticTypeVHW>();
    if (formatter == "GGA") return std::make_unique<StaticTypeGGA>();
    return nullptr;
}

}  // namespace nmea
