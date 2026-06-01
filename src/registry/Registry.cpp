#include "registry/Registry.hpp"

namespace nmea {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {
FieldDef f64(std::string name, std::string unit = {}) {
    return {std::move(name), FieldType::Float64, std::move(unit)};
}
FieldDef u32(std::string name, std::string unit = {}) {
    return {std::move(name), FieldType::UInt32, std::move(unit)};
}
FieldDef i32(std::string name, std::string unit = {}) {
    return {std::move(name), FieldType::Int32, std::move(unit)};
}
FieldDef ch(std::string name) {
    return {std::move(name), FieldType::Char, {}};
}
FieldDef str(std::string name) {
    return {std::move(name), FieldType::String, {}};
}
}  // namespace

// ---------------------------------------------------------------------------
// Definiciones internas — una por formatter relevante (D2).
// Añadir más sin modificar Registry: solo extender este bloque o llamar add().
// ---------------------------------------------------------------------------
static std::vector<SentenceDef> builtinDefs() {
    return {
        // ── GPS ──────────────────────────────────────────────────────────────
        {"GGA", Category::GPS, {
            f64("utc_time",          "hhmmss.ss"),
            f64("latitude",          "ddmm.mmm"),
            ch("ns_indicator"),
            f64("longitude",         "ddmm.mmm"),
            ch("ew_indicator"),
            u32("fix_quality"),
            u32("num_satellites"),
            f64("hdop"),
            f64("altitude",          "m"),
            ch("altitude_unit"),
            f64("geoid_separation",  "m"),
            ch("geoid_unit"),
            f64("dgps_age",          "s"),
            u32("dgps_station_id"),
        }},
        {"RMC", Category::GPS, {
            f64("utc_time",    "hhmmss.ss"),
            ch("status"),
            f64("latitude",    "ddmm.mmm"),
            ch("ns_indicator"),
            f64("longitude",   "ddmm.mmm"),
            ch("ew_indicator"),
            f64("speed",       "knots"),
            f64("course",      "deg"),
            u32("date"),
            f64("mag_var",     "deg"),
            ch("mag_var_dir"),
        }},
        {"VTG", Category::GPS, {
            f64("course_true",      "deg"),
            ch("true_ref"),
            f64("course_magnetic",  "deg"),
            ch("magnetic_ref"),
            f64("speed_knots",      "knots"),
            ch("knots_unit"),
            f64("speed_kmh",        "km/h"),
            ch("kmh_unit"),
        }},
        {"GLL", Category::GPS, {
            f64("latitude",    "ddmm.mmm"),
            ch("ns_indicator"),
            f64("longitude",   "ddmm.mmm"),
            ch("ew_indicator"),
            f64("utc_time",    "hhmmss.ss"),
            ch("status"),
        }},
        {"ZDA", Category::GPS, {
            f64("utc_time",     "hhmmss.ss"),
            u32("day"),
            u32("month"),
            u32("year"),
            i32("tz_hours"),
            i32("tz_minutes"),
        }},
        {"GSA", Category::GPS, {
            ch("mode"),
            u32("fix_type"),
            u32("sv1"),  u32("sv2"),  u32("sv3"),  u32("sv4"),
            u32("sv5"),  u32("sv6"),  u32("sv7"),  u32("sv8"),
            u32("sv9"),  u32("sv10"), u32("sv11"), u32("sv12"),
            f64("pdop"),
            f64("hdop"),
            f64("vdop"),
        }},
        {"GSV", Category::GPS, {
            u32("total_msgs"),
            u32("msg_num"),
            u32("total_sv"),
            u32("sv1_prn"),  f64("sv1_elev", "deg"), f64("sv1_azim", "deg"), u32("sv1_snr"),
            u32("sv2_prn"),  f64("sv2_elev", "deg"), f64("sv2_azim", "deg"), u32("sv2_snr"),
            u32("sv3_prn"),  f64("sv3_elev", "deg"), f64("sv3_azim", "deg"), u32("sv3_snr"),
            u32("sv4_prn"),  f64("sv4_elev", "deg"), f64("sv4_azim", "deg"), u32("sv4_snr"),
        }},
        {"XTE", Category::GPS, {
            ch("status_a"),
            ch("status_b"),
            f64("cross_track_error", "nm"),
            ch("steer_dir"),
            ch("units"),
        }},
        {"RMB", Category::GPS, {
            ch("status"),
            f64("xte",          "nm"),
            ch("steer_dir"),
            str("origin_wp"),
            str("dest_wp"),
            f64("dest_lat",     "ddmm.mmm"),
            ch("ns_indicator"),
            f64("dest_lon",     "ddmm.mmm"),
            ch("ew_indicator"),
            f64("range",        "nm"),
            f64("bearing",      "deg"),
            f64("vmg",          "knots"),
            ch("arrival_alarm"),
        }},
        {"DTM", Category::GPS, {
            str("datum"),
            str("subcode"),
            f64("lat_offset",   "min"),
            ch("ns"),
            f64("lon_offset",   "min"),
            ch("ew"),
            f64("alt_offset",   "m"),
            str("ref_datum"),
        }},

        // ── Weather ───────────────────────────────────────────────────────────
        {"MWD", Category::Weather, {
            f64("wind_dir_true",     "deg"),
            ch("true_ref"),
            f64("wind_dir_magnetic", "deg"),
            ch("mag_ref"),
            f64("wind_speed_knots",  "knots"),
            ch("knots_unit"),
            f64("wind_speed_ms",     "m/s"),
            ch("ms_unit"),
        }},
        {"MWV", Category::Weather, {
            f64("wind_angle",   "deg"),
            ch("reference"),
            f64("wind_speed",   "knots"),
            ch("speed_unit"),
            ch("status"),
        }},
        {"VWR", Category::Weather, {
            f64("wind_angle",       "deg"),
            ch("bow_dir"),
            f64("wind_speed_knots", "knots"),
            ch("knots_unit"),
            f64("wind_speed_ms",    "m/s"),
            ch("ms_unit"),
            f64("wind_speed_kmh",   "km/h"),
            ch("kmh_unit"),
        }},
        {"VWT", Category::Weather, {
            f64("wind_angle",       "deg"),
            ch("bow_dir"),
            f64("wind_speed_knots", "knots"),
            ch("knots_unit"),
            f64("wind_speed_ms",    "m/s"),
            ch("ms_unit"),
            f64("wind_speed_kmh",   "km/h"),
            ch("kmh_unit"),
        }},
        {"MTW", Category::Weather, {
            f64("temperature", "C"),
            ch("unit"),
        }},

        // ── Heading ───────────────────────────────────────────────────────────
        {"HDG", Category::Heading, {
            f64("heading",         "deg"),
            f64("mag_dev",         "deg"),
            ch("dev_dir"),
            f64("mag_var",         "deg"),
            ch("var_dir"),
        }},
        {"HDT", Category::Heading, {
            f64("heading_true",    "deg"),
            ch("true_ref"),
        }},
        {"HDM", Category::Heading, {
            f64("heading_magnetic","deg"),
            ch("mag_ref"),
        }},
        {"THS", Category::Heading, {
            f64("heading_true",    "deg"),
            ch("mode"),
        }},
        {"ROT", Category::Heading, {
            f64("rate_of_turn",    "deg/min"),
            ch("status"),
        }},
        {"RSA", Category::Heading, {
            f64("rudder_angle_1",  "deg"),
            ch("status_1"),
            f64("rudder_angle_2",  "deg"),
            ch("status_2"),
        }},

        // ── Radar ─────────────────────────────────────────────────────────────
        {"TLL", Category::Radar, {
            u32("target_num"),
            f64("latitude",  "ddmm.mmm"),
            ch("ns_indicator"),
            f64("longitude", "ddmm.mmm"),
            ch("ew_indicator"),
            str("target_name"),
            f64("utc_time",  "hhmmss.ss"),
            ch("status"),
            ch("ref"),
        }},
        {"TTM", Category::Radar, {
            u32("target_num"),
            f64("distance",   "nm"),
            f64("bearing",    "deg"),
            ch("bearing_ref"),
            f64("speed",      "knots"),
            f64("course",     "deg"),
            ch("course_ref"),
            f64("cpa",        "nm"),
            f64("tcpa",       "min"),
            ch("speed_unit"),
            str("target_name"),
            ch("status"),
            ch("ref"),
        }},
        {"OSD", Category::Radar, {
            f64("heading",       "deg"),
            ch("heading_status"),
            f64("vessel_course", "deg"),
            ch("course_ref"),
            f64("vessel_speed",  "knots"),
            ch("speed_ref"),
            f64("set",           "deg"),
            f64("drift",         "knots"),
            ch("speed_unit"),
        }},

        // ── Sounder ───────────────────────────────────────────────────────────
        {"DBT", Category::Sounder, {
            f64("depth_feet",   "ft"),
            ch("feet_unit"),
            f64("depth_meters", "m"),
            ch("meters_unit"),
            f64("depth_fathoms","fathom"),
            ch("fathoms_unit"),
        }},
        {"DPT", Category::Sounder, {
            f64("depth",        "m"),
            f64("offset",       "m"),
            f64("max_range",    "m"),
        }},
        {"DBK", Category::Sounder, {
            f64("depth_feet",   "ft"),
            ch("feet_unit"),
            f64("depth_meters", "m"),
            ch("meters_unit"),
            f64("depth_fathoms","fathom"),
            ch("fathoms_unit"),
        }},
        {"DBS", Category::Sounder, {
            f64("depth_feet",   "ft"),
            ch("feet_unit"),
            f64("depth_meters", "m"),
            ch("meters_unit"),
            f64("depth_fathoms","fathom"),
            ch("fathoms_unit"),
        }},

        // ── Velocity ──────────────────────────────────────────────────────────
        {"VHW", Category::Velocity, {
            f64("heading_true",     "deg"),
            ch("true_ref"),
            f64("heading_magnetic", "deg"),
            ch("mag_ref"),
            f64("speed_knots",      "knots"),
            ch("knots_unit"),
            f64("speed_kmh",        "km/h"),
            ch("kmh_unit"),
        }},
        {"VLW", Category::Velocity, {
            f64("total_water_dist",   "nm"),
            ch("total_unit"),
            f64("ground_dist",        "nm"),
            ch("ground_unit"),
        }},
        {"VBW", Category::Velocity, {
            f64("longitudinal_water", "knots"),
            f64("transverse_water",   "knots"),
            ch("water_status"),
            f64("longitudinal_gnd",   "knots"),
            f64("transverse_gnd",     "knots"),
            ch("ground_status"),
        }},

        // ── Inertial / propietarias VectorNav ─────────────────────────────────
        // Las sentencias $VN... no tienen talker estándar: address=="VNYMR", etc. (D2)
        {"VNYMR", Category::Inertial, {
            f64("magnetic_heading", "deg"),
            f64("pitch",            "deg"),
            f64("roll",             "deg"),
        }},
        {"VNYPR", Category::Inertial, {
            f64("yaw",   "deg"),
            f64("pitch", "deg"),
            f64("roll",  "deg"),
        }},
        {"VNYQT", Category::Inertial, {
            f64("quaternion_x"),
            f64("quaternion_y"),
            f64("quaternion_z"),
            f64("quaternion_w"),
        }},
        {"VNYCM", Category::Inertial, {
            f64("mag_x",   "gauss"),
            f64("mag_y",   "gauss"),
            f64("mag_z",   "gauss"),
            f64("accel_x", "m/s2"),
            f64("accel_y", "m/s2"),
            f64("accel_z", "m/s2"),
        }},
        {"VNYIA", Category::Inertial, {
            f64("accel_x",  "m/s2"),
            f64("accel_y",  "m/s2"),
            f64("accel_z",  "m/s2"),
            f64("gyro_x",   "rad/s"),
            f64("gyro_y",   "rad/s"),
            f64("gyro_z",   "rad/s"),
        }},

        // ── Autopilot ─────────────────────────────────────────────────────────
        {"APB", Category::Autopilot, {
            ch("status_general"),
            ch("status_cycle_lock"),
            f64("xte_magnitude",       "nm"),
            ch("steer_direction"),         // L/R
            ch("xte_units"),               // N
            ch("arrival_circle"),          // A
            ch("perpendicular_passed"),    // A
            f64("bearing_origin_dest", "deg"),
            ch("bearing_origin_ref"),      // M/T
            str("dest_waypoint_id"),
            f64("bearing_present_dest","deg"),
            ch("bearing_present_ref"),     // M/T
            f64("heading_to_steer",    "deg"),
            ch("heading_to_steer_ref"),    // M/T
            ch("mode"),
        }},

        // ── Engine / propulsión ───────────────────────────────────────────────
        {"RPM", Category::Engine, {
            ch("source"),          // S=eje, E=motor
            i32("source_number"),
            f64("speed",   "rpm"),
            f64("pitch",   "%"),
            ch("status"),          // A=válido
        }},

        // ── AIS (encapsulación, delimitador '!'): VDM=otros buques, VDO=propio ──
        // address "AIVDM"/"AIVDO" → resolve quita el talker "AI" → "VDM"/"VDO".
        {"VDM", Category::AIS, {
            u32("total_sentences"),
            u32("sentence_number"),
            u32("sequential_msg_id"),
            ch("channel"),                 // A/B
            str("payload"),                // ASCII de 6 bits (mensaje AIS)
            u32("fill_bits"),
        }},
        {"VDO", Category::AIS, {
            u32("total_sentences"),
            u32("sentence_number"),
            u32("sequential_msg_id"),
            ch("channel"),
            str("payload"),
            u32("fill_bits"),
        }},
    };
}

// ---------------------------------------------------------------------------
// Registry implementation
// ---------------------------------------------------------------------------
Registry Registry::builtin() {
    Registry r;
    for (auto& def : builtinDefs())
        r.add(std::move(def));
    return r;
}

const SentenceDef* Registry::lookup(std::string_view formatter) const noexcept {
    const auto it = defs_.find(std::string(formatter));
    return it != defs_.end() ? &it->second : nullptr;
}

void Registry::add(SentenceDef def) {
    const std::string key = def.formatter;
    defs_.insert_or_assign(key, std::move(def));
}

const char* category_name(Category c) noexcept {
    switch (c) {
        case Category::GPS:      return "GPS";
        case Category::Weather:  return "Weather";
        case Category::Heading:  return "Heading";
        case Category::Radar:    return "Radar";
        case Category::Sounder:  return "Sounder";
        case Category::Velocity:  return "Velocity";
        case Category::Attitude:  return "Attitude";
        case Category::Inertial:  return "Inertial";
        case Category::Autopilot: return "Autopilot";
        case Category::Engine:    return "Engine";
        case Category::AIS:       return "AIS";
    }
    return "Unknown";
}

}  // namespace nmea
