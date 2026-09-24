#include "mapper/Mapper.hpp"

#include <charconv>
#include <chrono>

#include <fastdds/dds/core/Types.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeBuilder.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeBuilderFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>
#include <fastdds/dds/xtypes/dynamic_types/TypeDescriptor.hpp>
#include <fastdds/dds/xtypes/dynamic_types/detail/dynamic_language_binding.hpp>

namespace nmea {

namespace {
std::string category_str(Category c) {
    switch (c) {
        case Category::GPS:      return "gps";
        case Category::Weather:  return "weather";
        case Category::Heading:  return "heading";
        case Category::Radar:    return "radar";
        case Category::Sounder:  return "sounder";
        case Category::Velocity:  return "velocity";
        case Category::Attitude:  return "attitude";
        case Category::Inertial:  return "inertial";
        case Category::Autopilot: return "autopilot";
        case Category::Engine:    return "engine";
        case Category::AIS:       return "ais";
    }
    return "unknown";
}
}  // namespace

using namespace eprosima::fastdds::dds;

// ---------------------------------------------------------------------------
// Helpers internos
// ---------------------------------------------------------------------------
namespace {

DynamicType::_ref_type primitive(TypeKind k) {
    return DynamicTypeBuilderFactory::get_instance()->get_primitive_type(k);
}

DynamicType::_ref_type string_type() {
    // LENGTH_UNLIMITED = -1; cast a uint32_t = 0xFFFFFFFF = UNBOUNDED en Fast DDS 3.
    constexpr uint32_t UNBOUNDED = static_cast<uint32_t>(eprosima::fastdds::dds::LENGTH_UNLIMITED);
    return DynamicTypeBuilderFactory::get_instance()
            ->create_string_type(UNBOUNDED)
            ->build();
}

// Convierte FieldType del registro al TypeKind de XTypes.
TypeKind to_kind(FieldType ft) {
    switch (ft) {
        case FieldType::Float64: return TK_FLOAT64;
        case FieldType::Int32:   return TK_INT32;
        case FieldType::UInt32:  return TK_UINT32;
        case FieldType::Char:    return TK_CHAR8;
        case FieldType::String:  return TK_STRING8;
    }
    return TK_FLOAT64;
}

// Parseo sin locale ni excepciones para campos de wire.
double parse_f64(std::string_view sv) noexcept {
    if (sv.empty()) return 0.0;
    double v = 0.0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}
int32_t parse_i32(std::string_view sv) noexcept {
    if (sv.empty()) return 0;
    int32_t v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}
uint32_t parse_u32(std::string_view sv) noexcept {
    if (sv.empty()) return 0u;
    uint32_t v = 0u;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}
char parse_char(std::string_view sv) noexcept {
    return sv.empty() ? '\0' : sv[0];
}

// Añade un miembro al builder. is_key=true solo para device_id.
void add_member(DynamicTypeBuilder::_ref_type& builder,
                std::string_view name,
                DynamicType::_ref_type type,
                bool is_key = false) {
    auto desc = traits<MemberDescriptor>::make_shared();
    desc->name(std::string(name));
    desc->type(type);
    desc->is_key(is_key);
    builder->add_member(desc);
}

// Agrega la cabecera común a todos los tipos.
// device_id es el @key DDS (D3/D5): identifica la instancia por dispositivo.
void add_common_header(DynamicTypeBuilder::_ref_type& builder) {
    auto str = string_type();
    add_member(builder, "device_id",       str,                   /*is_key=*/true);
    add_member(builder, "talker",          str);
    add_member(builder, "recv_timestamp",  primitive(TK_INT64));
}

}  // namespace

// ---------------------------------------------------------------------------
// Mapper
// ---------------------------------------------------------------------------
Mapper::Mapper(const Registry& registry) : registry_(registry) {}

std::string Mapper::resolve_formatter(std::string_view address) const {
    // 1. Prueba el address completo (cubre propietarias como "VNYMR").
    if (registry_.lookup(address)) return std::string(address);
    // 2. Intenta retirar los 2 chars de talker estándar.
    if (address.size() > 2) {
        const std::string fmt(address.substr(2));
        if (registry_.lookup(fmt)) return fmt;
    }
    return {};  // desconocido → RawSentence
}

DynamicType::_ref_type Mapper::build_raw_sentence_type() const {
    auto desc = traits<TypeDescriptor>::make_shared();
    desc->kind(TK_STRUCTURE);
    desc->name("RawSentence");
    auto builder = DynamicTypeBuilderFactory::get_instance()->create_type(desc);
    add_common_header(builder);
    add_member(builder, "address", string_type());
    add_member(builder, "payload", string_type());
    return builder->build();
}

DynamicType::_ref_type Mapper::build_type(const SentenceDef& def) const {
    auto desc = traits<TypeDescriptor>::make_shared();
    desc->kind(TK_STRUCTURE);
    desc->name("Nmea" + def.formatter);  // p.e. "NmeaGGA"
    auto builder = DynamicTypeBuilderFactory::get_instance()->create_type(desc);

    add_common_header(builder);

    for (const auto& field : def.fields) {
        if (field.type == FieldType::String) {
            add_member(builder, field.name, string_type());
        } else {
            add_member(builder, field.name, primitive(to_kind(field.type)));
        }
    }
    return builder->build();
}

DynamicType::_ref_type Mapper::type_for(std::string_view formatter) const {
    const std::string key(formatter);
    auto it = type_cache_.find(key);
    if (it != type_cache_.end()) return it->second;

    const SentenceDef* def = registry_.lookup(formatter);
    DynamicType::_ref_type t = def ? build_type(*def) : raw_sentence_type();
    type_cache_.emplace(key, t);
    return t;
}

DynamicType::_ref_type Mapper::raw_sentence_type() const {
    if (!raw_type_) raw_type_ = build_raw_sentence_type();
    return raw_type_;
}

void Mapper::populate(DynamicData::_ref_type& data,
                       const SentenceView& view,
                       std::string_view device_id,
                       int64_t recv_ns) const {
    if (recv_ns == 0) {
        recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    }

    const std::string formatter = resolve_formatter(view.address);
    populate(data, view, device_id, formatter, recv_ns);
}

void Mapper::populate(DynamicData::_ref_type& data,
                      const SentenceView& view,
                      std::string_view device_id,
                      std::string_view formatter_sv,
                      int64_t recv_ns) const {
    if (recv_ns == 0) {
        recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    }

    const std::string formatter(formatter_sv);
    const SentenceDef* def = formatter.empty() ? nullptr : registry_.lookup(formatter);

    std::string talker;
    if (!formatter.empty() && formatter != std::string(view.address)) {
        talker = std::string(view.address.substr(0, view.address.size() - formatter.size()));
    }

    data->set_string_value(data->get_member_id_by_name("device_id"),     std::string(device_id));
    data->set_string_value(data->get_member_id_by_name("talker"),        talker);
    data->set_int64_value( data->get_member_id_by_name("recv_timestamp"), recv_ns);

    if (!def) {
        data->set_string_value(data->get_member_id_by_name("address"),
                               std::string(view.address));
        std::string payload;
        for (std::size_t i = 0; i < view.fields.size(); ++i) {
            if (i) payload += ',';
            payload += view.fields[i];
        }
        data->set_string_value(data->get_member_id_by_name("payload"), payload);
        return;
    }

    const std::size_t n = std::min(view.fields.size(), def->fields.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto& fd  = def->fields[i];
        const auto  sv  = view.fields[i];
        const MemberId mid = data->get_member_id_by_name(fd.name);
        if (mid == MEMBER_ID_INVALID) continue;
        switch (fd.type) {
            case FieldType::Float64: data->set_float64_value(mid, parse_f64(sv)); break;
            case FieldType::Int32:   data->set_int32_value(mid,   parse_i32(sv)); break;
            case FieldType::UInt32:  data->set_uint32_value(mid,  parse_u32(sv)); break;
            case FieldType::Char:    data->set_char8_value(mid,   parse_char(sv)); break;
            case FieldType::String:  data->set_string_value(mid,  std::string(sv)); break;
        }
    }
}

DynamicData::_ref_type Mapper::map(const SentenceView& view,
                                    std::string_view device_id,
                                    int64_t recv_ns) const {
    const std::string formatter = resolve_formatter(view.address);
    const SentenceDef* def = formatter.empty() ? nullptr : registry_.lookup(formatter);
    DynamicType::_ref_type dyn_type = def ? type_for(formatter) : raw_sentence_type();
    DynamicData::_ref_type data =
            DynamicDataFactory::get_instance()->create_data(dyn_type);
    populate(data, view, device_id, formatter, recv_ns);
    return data;
}

Mapper::SentenceInfo Mapper::resolve(std::string_view address) const {
    const std::string fmt = resolve_formatter(address);
    if (fmt.empty())
        return {"", "", "nmea/raw/RawSentence", "RawSentence"};
    // talker = address menos el sufijo formatter (vacío si propietario, ej. "VNYMR").
    std::string talker;
    if (address.size() > fmt.size())
        talker = std::string(address.substr(0, address.size() - fmt.size()));
    const SentenceDef* def = registry_.lookup(fmt);
    return {fmt, talker,
            "nmea/" + category_str(def->category) + "/" + fmt,
            "Nmea" + fmt};
}

}  // namespace nmea
