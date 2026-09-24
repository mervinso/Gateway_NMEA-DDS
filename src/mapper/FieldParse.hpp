// src/mapper/FieldParse.hpp — conversión de campos de wire, compartida por los
// dos brazos del experimento.
//
// Vive en una cabecera y no dentro de `Mapper.cpp` porque el brazo generado
// tiene que convertir los campos **exactamente igual** que el dinámico. Dos
// implementaciones de esto es como los dos brazos acaban produciendo CDR
// distinto para la misma sentencia, y la compuerta de equivalencia falla por una
// diferencia de redondeo en vez de por algo que importe.
//
// Sin locale y sin excepciones: son campos de wire y esto está en el camino
// caliente.
//
// Ninguna de estas funciones recibe ya una cadena vacía desde `populate()`: un
// campo vacío se deja sin convertir y su bit de `field_presence` queda en cero
// (tesis §8.5.3). Las guardas se conservan porque las funciones son alcanzables
// desde otros llamadores, no porque sean el camino de un campo nulo.

#pragma once

#include <charconv>
#include <cstdint>
#include <string_view>

namespace nmea {

inline double parse_f64(std::string_view sv) noexcept {
    if (sv.empty()) return 0.0;
    double v = 0.0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}

inline std::int32_t parse_i32(std::string_view sv) noexcept {
    if (sv.empty()) return 0;
    std::int32_t v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}

inline std::uint32_t parse_u32(std::string_view sv) noexcept {
    if (sv.empty()) return 0u;
    std::uint32_t v = 0u;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}

inline char parse_char(std::string_view sv) noexcept {
    return sv.empty() ? '\0' : sv[0];
}

}  // namespace nmea
