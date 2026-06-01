#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nmea {

enum class Category {
    GPS,
    Weather,
    Heading,
    Radar,
    Sounder,
    Velocity,
    Attitude,
    Inertial,
};

enum class FieldType {
    Float64,
    Int32,
    UInt32,
    String,
    Char,
};

struct FieldDef {
    std::string name;
    FieldType type;
    std::string unit;  // vacío si adimensional
};

struct SentenceDef {
    std::string formatter;  // token dirección completo: "GGA", "VNYMR", etc.
    Category category;
    std::vector<FieldDef> fields;
};

// Catálogo de sentencias NMEA. Cold path: usa heap libremente.
// Fuente de verdad para detección de tipo (D4) y preview IDL (D14).
class Registry {
public:
    // Crea el registro pre-cargado con todas las definiciones internas (D2).
    static Registry builtin();

    // Devuelve nullptr si el formatter no está registrado.
    const SentenceDef* lookup(std::string_view formatter) const noexcept;

    // Añade o reemplaza una definición (extensión en runtime sin recompilar).
    void add(SentenceDef def);

    std::size_t size() const noexcept { return defs_.size(); }

private:
    std::unordered_map<std::string, SentenceDef> defs_;
};

// Devuelve el nombre en texto de una categoría: "GPS", "Weather", etc.
std::string category_name(Category c) noexcept;

}  // namespace nmea
