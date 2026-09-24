// GENERADO POR tools/idl_from_registry -- NO EDITAR A MANO.
//
// Poblado del brazo generado. Una funcion por tipo, emitida desde el registro
// igual que el IDL, para que el brazo estatico y el dinamico no puedan
// divergir. Escribir estas a mano seria escribir cinco copias de una regla que
// ya existe en un sitio.
//
// La regla es la de §8.5.3: un campo recibido con contenido se convierte y pone
// su bit en field_presence; uno recibido vacio y uno que no llego se dejan en su
// valor por defecto con el bit en cero. Identica a la de Mapper::populate().
//
// Las conversiones salen de mapper/FieldParse.hpp, las mismas que usa el brazo
// dinamico.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "mapper/FieldParse.hpp"
#include "parser/Parser.hpp"

#include "NmeaROT.hpp"
#include "NmeaRSA.hpp"
#include "NmeaDBT.hpp"
#include "NmeaVHW.hpp"
#include "NmeaGGA.hpp"

namespace nmea {

inline void populate_static(NmeaROT& s,
                            const SentenceView& v,
                            std::string_view device_id,
                            std::string_view talker,
                            std::int64_t recv_ns) {
    s.device_id(std::string(device_id));
    s.talker(std::string(talker));
    s.recv_timestamp(recv_ns);
    std::uint32_t presence = 0;
    if (v.fields.size() > 0 && !v.fields[0].empty()) {
        s.rate_of_turn(parse_f64(v.fields[0]));
        presence |= (1u << 0);
    }
    if (v.fields.size() > 1 && !v.fields[1].empty()) {
        s.status(parse_char(v.fields[1]));
        presence |= (1u << 1);
    }
    s.field_presence(presence);
}

inline void populate_static(NmeaRSA& s,
                            const SentenceView& v,
                            std::string_view device_id,
                            std::string_view talker,
                            std::int64_t recv_ns) {
    s.device_id(std::string(device_id));
    s.talker(std::string(talker));
    s.recv_timestamp(recv_ns);
    std::uint32_t presence = 0;
    if (v.fields.size() > 0 && !v.fields[0].empty()) {
        s.rudder_angle_1(parse_f64(v.fields[0]));
        presence |= (1u << 0);
    }
    if (v.fields.size() > 1 && !v.fields[1].empty()) {
        s.status_1(parse_char(v.fields[1]));
        presence |= (1u << 1);
    }
    if (v.fields.size() > 2 && !v.fields[2].empty()) {
        s.rudder_angle_2(parse_f64(v.fields[2]));
        presence |= (1u << 2);
    }
    if (v.fields.size() > 3 && !v.fields[3].empty()) {
        s.status_2(parse_char(v.fields[3]));
        presence |= (1u << 3);
    }
    s.field_presence(presence);
}

inline void populate_static(NmeaDBT& s,
                            const SentenceView& v,
                            std::string_view device_id,
                            std::string_view talker,
                            std::int64_t recv_ns) {
    s.device_id(std::string(device_id));
    s.talker(std::string(talker));
    s.recv_timestamp(recv_ns);
    std::uint32_t presence = 0;
    if (v.fields.size() > 0 && !v.fields[0].empty()) {
        s.depth_feet(parse_f64(v.fields[0]));
        presence |= (1u << 0);
    }
    if (v.fields.size() > 1 && !v.fields[1].empty()) {
        s.feet_unit(parse_char(v.fields[1]));
        presence |= (1u << 1);
    }
    if (v.fields.size() > 2 && !v.fields[2].empty()) {
        s.depth_meters(parse_f64(v.fields[2]));
        presence |= (1u << 2);
    }
    if (v.fields.size() > 3 && !v.fields[3].empty()) {
        s.meters_unit(parse_char(v.fields[3]));
        presence |= (1u << 3);
    }
    if (v.fields.size() > 4 && !v.fields[4].empty()) {
        s.depth_fathoms(parse_f64(v.fields[4]));
        presence |= (1u << 4);
    }
    if (v.fields.size() > 5 && !v.fields[5].empty()) {
        s.fathoms_unit(parse_char(v.fields[5]));
        presence |= (1u << 5);
    }
    s.field_presence(presence);
}

inline void populate_static(NmeaVHW& s,
                            const SentenceView& v,
                            std::string_view device_id,
                            std::string_view talker,
                            std::int64_t recv_ns) {
    s.device_id(std::string(device_id));
    s.talker(std::string(talker));
    s.recv_timestamp(recv_ns);
    std::uint32_t presence = 0;
    if (v.fields.size() > 0 && !v.fields[0].empty()) {
        s.heading_true(parse_f64(v.fields[0]));
        presence |= (1u << 0);
    }
    if (v.fields.size() > 1 && !v.fields[1].empty()) {
        s.true_ref(parse_char(v.fields[1]));
        presence |= (1u << 1);
    }
    if (v.fields.size() > 2 && !v.fields[2].empty()) {
        s.heading_magnetic(parse_f64(v.fields[2]));
        presence |= (1u << 2);
    }
    if (v.fields.size() > 3 && !v.fields[3].empty()) {
        s.mag_ref(parse_char(v.fields[3]));
        presence |= (1u << 3);
    }
    if (v.fields.size() > 4 && !v.fields[4].empty()) {
        s.speed_knots(parse_f64(v.fields[4]));
        presence |= (1u << 4);
    }
    if (v.fields.size() > 5 && !v.fields[5].empty()) {
        s.knots_unit(parse_char(v.fields[5]));
        presence |= (1u << 5);
    }
    if (v.fields.size() > 6 && !v.fields[6].empty()) {
        s.speed_kmh(parse_f64(v.fields[6]));
        presence |= (1u << 6);
    }
    if (v.fields.size() > 7 && !v.fields[7].empty()) {
        s.kmh_unit(parse_char(v.fields[7]));
        presence |= (1u << 7);
    }
    s.field_presence(presence);
}

inline void populate_static(NmeaGGA& s,
                            const SentenceView& v,
                            std::string_view device_id,
                            std::string_view talker,
                            std::int64_t recv_ns) {
    s.device_id(std::string(device_id));
    s.talker(std::string(talker));
    s.recv_timestamp(recv_ns);
    std::uint32_t presence = 0;
    if (v.fields.size() > 0 && !v.fields[0].empty()) {
        s.utc_time(parse_f64(v.fields[0]));
        presence |= (1u << 0);
    }
    if (v.fields.size() > 1 && !v.fields[1].empty()) {
        s.latitude(parse_f64(v.fields[1]));
        presence |= (1u << 1);
    }
    if (v.fields.size() > 2 && !v.fields[2].empty()) {
        s.ns_indicator(parse_char(v.fields[2]));
        presence |= (1u << 2);
    }
    if (v.fields.size() > 3 && !v.fields[3].empty()) {
        s.longitude(parse_f64(v.fields[3]));
        presence |= (1u << 3);
    }
    if (v.fields.size() > 4 && !v.fields[4].empty()) {
        s.ew_indicator(parse_char(v.fields[4]));
        presence |= (1u << 4);
    }
    if (v.fields.size() > 5 && !v.fields[5].empty()) {
        s.fix_quality(parse_u32(v.fields[5]));
        presence |= (1u << 5);
    }
    if (v.fields.size() > 6 && !v.fields[6].empty()) {
        s.num_satellites(parse_u32(v.fields[6]));
        presence |= (1u << 6);
    }
    if (v.fields.size() > 7 && !v.fields[7].empty()) {
        s.hdop(parse_f64(v.fields[7]));
        presence |= (1u << 7);
    }
    if (v.fields.size() > 8 && !v.fields[8].empty()) {
        s.altitude(parse_f64(v.fields[8]));
        presence |= (1u << 8);
    }
    if (v.fields.size() > 9 && !v.fields[9].empty()) {
        s.altitude_unit(parse_char(v.fields[9]));
        presence |= (1u << 9);
    }
    if (v.fields.size() > 10 && !v.fields[10].empty()) {
        s.geoid_separation(parse_f64(v.fields[10]));
        presence |= (1u << 10);
    }
    if (v.fields.size() > 11 && !v.fields[11].empty()) {
        s.geoid_unit(parse_char(v.fields[11]));
        presence |= (1u << 11);
    }
    if (v.fields.size() > 12 && !v.fields[12].empty()) {
        s.dgps_age(parse_f64(v.fields[12]));
        presence |= (1u << 12);
    }
    if (v.fields.size() > 13 && !v.fields[13].empty()) {
        s.dgps_station_id(parse_u32(v.fields[13]));
        presence |= (1u << 13);
    }
    s.field_presence(presence);
}

}  // namespace nmea
