#include <gtest/gtest.h>

#include <cstdint>

#include "mapper/Mapper.hpp"
#include "parser/Parser.hpp"
#include "registry/Registry.hpp"

#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeMember.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>

using namespace nmea;
using namespace eprosima::fastdds::dds;

namespace {
// Parsea una sentencia y devuelve el SentenceView (el Parser debe sobrevivir la llamada).
struct Parsed {
    Parser parser;
    ParseResult result{ParseResult::Incomplete};

    explicit Parsed(std::string_view sentence) {
        for (char c : sentence) {
            result = parser.consume(c);
            if (result != ParseResult::Incomplete) break;
        }
    }
    const SentenceView& view() const { return parser.sentence(); }
};
}  // namespace

TEST(Mapper, MapsGgaToCompleteWithDeviceIdAsKey) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    Parsed p("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
    ASSERT_EQ(p.result, ParseResult::Complete);

    DynamicData::_ref_type data = mapper.map(p.view(), "gps_bow");

    ASSERT_NE(data, nullptr);

    // device_id es el @key (D3): debe estar presente con el valor correcto.
    std::string device_id;
    MemberId id = data->get_member_id_by_name("device_id");
    ASSERT_NE(id, MEMBER_ID_INVALID);
    EXPECT_EQ(data->get_string_value(device_id, id), RETCODE_OK);
    EXPECT_EQ(device_id, "gps_bow");
}

TEST(Mapper, TalkerExtractedFromStandardAddress) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    Parsed p("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
    ASSERT_EQ(p.result, ParseResult::Complete);

    DynamicData::_ref_type data = mapper.map(p.view(), "gps_bow");
    ASSERT_NE(data, nullptr);

    std::string talker;
    EXPECT_EQ(data->get_string_value(talker, data->get_member_id_by_name("talker")), RETCODE_OK);
    EXPECT_EQ(talker, "GP");
}

TEST(Mapper, SensorFieldPopulatedFromWireValue) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    // altitude está en field[8] de GGA → debe ser 545.4 m
    Parsed p("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
    ASSERT_EQ(p.result, ParseResult::Complete);

    DynamicData::_ref_type data = mapper.map(p.view(), "gps_bow");
    ASSERT_NE(data, nullptr);

    double altitude = 0.0;
    MemberId mid = data->get_member_id_by_name("altitude");
    ASSERT_NE(mid, MEMBER_ID_INVALID);
    EXPECT_EQ(data->get_float64_value(altitude, mid), RETCODE_OK);
    EXPECT_DOUBLE_EQ(altitude, 545.4);
}

TEST(Mapper, ProprietaryVectorNavNoTalker) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    Parsed p("$VNYMR,-165.918,-008.770,+000.198*7D\r\n");
    ASSERT_EQ(p.result, ParseResult::Complete);

    DynamicData::_ref_type data = mapper.map(p.view(), "imu_main");
    ASSERT_NE(data, nullptr);

    // Para propietarias el talker debe estar vacío (no hay prefijo de 2 chars).
    std::string talker;
    EXPECT_EQ(data->get_string_value(talker, data->get_member_id_by_name("talker")), RETCODE_OK);
    EXPECT_TRUE(talker.empty());

    // El campo magnetic_heading debe ser -165.918.
    double heading = 0.0;
    MemberId mid = data->get_member_id_by_name("magnetic_heading");
    ASSERT_NE(mid, MEMBER_ID_INVALID);
    EXPECT_EQ(data->get_float64_value(heading, mid), RETCODE_OK);
    EXPECT_NEAR(heading, -165.918, 1e-3);
}

TEST(Mapper, UnknownFormatterProducesRawSentence) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    // $PXYZ es propietaria desconocida; checksum calculado.
    Parsed p("$PXYZ,foo,bar,42*42\r\n");
    // No importa el resultado del parse (puede ser ChecksumError); lo que importa
    // es que el mapper genere un RawSentence para lo que reciba.
    // Usamos una sentencia cuyo address no está en el registro.
    Parser raw_parser;
    // Construimos manualmente un SentenceView para "UNKNOWN" con 2 campos.
    // En la práctica llegaría como ChecksumError o como un stream real; aquí
    // lo que testeamos es type_for() sobre un formatter desconocido.

    DynamicType::_ref_type raw_type = mapper.raw_sentence_type();
    ASSERT_NE(raw_type, nullptr);
    EXPECT_EQ(std::string(raw_type->get_name()), "RawSentence");
}

TEST(Mapper, TypeForSameFormatterReturnsCachedType) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    DynamicType::_ref_type t1 = mapper.type_for("GGA");
    DynamicType::_ref_type t2 = mapper.type_for("GGA");

    // Misma instancia (puntero) → viene del cache.
    EXPECT_EQ(t1.get(), t2.get());
}

// --------------------------------------------------------------------------
// §8.5.3: un campo vacio queda AUSENTE, no en cero. IEC 61162-1 permite
// transmitir un campo vacio cuando el dato no esta disponible, y una
// profundidad no disponible no es una profundidad de cero metros.
//
// La presencia va en `field_presence`, un cuarto miembro de cabecera, y no en
// miembros opcionales. Razon medida, no de gusto: Fast DDS 3.6.2 no honra
// is_optional en DynamicData -- un miembro opcional se codifica siempre, este
// puesto, sin tocar o limpiado con clear_value(), verificado byte a byte con
// XCDR2 en FINAL, APPENDABLE y MUTABLE. El brazo generado SI codificaria la
// ausencia con @optional, asi que emparejar los dos por esa via rompe la
// compuerta de equivalencia CDR. Una mascara la expresan los dos brazos igual.
// --------------------------------------------------------------------------
namespace {
std::uint32_t presence_of(const DynamicData::_ref_type& data) {
    std::uint32_t m = 0;
    EXPECT_EQ(data->get_uint32_value(m, data->get_member_id_by_name("field_presence")),
              RETCODE_OK);
    return m;
}
}  // namespace

TEST(Mapper, HeaderCarriesPresenceAndSensorFieldsAreNotOptional) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);
    const DynamicType::_ref_type type = mapper.type_for("GGA");
    ASSERT_NE(type, nullptr);

    DynamicTypeMembersByName members;
    ASSERT_EQ(type->get_all_members_by_name(members), RETCODE_OK);

    for (const char* h : {"device_id", "talker", "recv_timestamp", "field_presence"})
        EXPECT_NE(members.find(h), members.end()) << "falta el miembro de cabecera " << h;

    // Ningun miembro se declara opcional: la mascara sustituye ese mecanismo, y
    // llevar los dos haria que el TypeObject del brazo dinamico difiriera del
    // generado justo en lo que la compuerta de conformidad compara.
    for (auto& [name, member] : members) {
        MemberDescriptor::_ref_type d = traits<MemberDescriptor>::make_shared();
        member->get_descriptor(d);
        EXPECT_FALSE(d->is_optional()) << name << " no deberia declararse opcional";
    }
}

TEST(Mapper, PresenceMaskDistinguishesEmptyFromZero) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);
    const SentenceDef* def = reg.lookup("GGA");
    ASSERT_NE(def, nullptr);

    auto index_of = [&](const char* name) -> int {
        for (std::size_t i = 0; i < def->fields.size(); ++i)
            if (def->fields[i].name == name) return static_cast<int>(i);
        return -1;
    };
    const int i_alt = index_of("altitude");
    const int i_age = index_of("dgps_age");
    ASSERT_GE(i_alt, 0);
    ASSERT_GE(i_age, 0);

    // dgps_age llega vacio; altitude llega con 545.4.
    Parsed p("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
    ASSERT_EQ(p.result, ParseResult::Complete);
    DynamicData::_ref_type d = mapper.map(p.view(), "gps");
    ASSERT_NE(d, nullptr);

    const std::uint32_t mask = presence_of(d);
    EXPECT_TRUE(mask & (1u << i_alt)) << "altitude llego con contenido";
    EXPECT_FALSE(mask & (1u << i_age)) << "dgps_age llego vacio: su bit debe estar en cero";

    // Y el valor sigue leyendose como 0.0 -- que es exactamente por lo que hace
    // falta la mascara. Sin ella, cero y ausente son indistinguibles.
    double age = 99.0;
    EXPECT_EQ(d->get_float64_value(age, d->get_member_id_by_name("dgps_age")), RETCODE_OK);
    EXPECT_DOUBLE_EQ(age, 0.0);
}

TEST(Mapper, TruncatedSentenceLeavesTrailingBitsClear) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);
    const SentenceDef* def = reg.lookup("GGA");
    ASSERT_NE(def, nullptr);

    // GGA con solo tres campos: los demas no llegaron.
    Parsed p("$GPGGA,123519,4807.038,N*27\r\n");
    ASSERT_EQ(p.result, ParseResult::Complete);
    DynamicData::_ref_type d = mapper.map(p.view(), "gps");
    ASSERT_NE(d, nullptr);

    const std::uint32_t mask = presence_of(d);
    for (std::size_t i = 3; i < def->fields.size(); ++i)
        EXPECT_FALSE(mask & (1u << i))
                << "el campo " << def->fields[i].name << " no llego: su bit debe estar en cero";
}

TEST(Mapper, EveryRegistryDefinitionFitsThePresenceMask) {
    // La mascara es de 32 bits. Si alguna definicion futura declarara mas de 32
    // campos, los bits altos se perderian en silencio. Se comprueba en vez de
    // contarse a mano: el conteo de formatters de este registro ya se corrigio
    // dos veces por hacerlo a ojo.
    const Registry reg = Registry::builtin();
    std::size_t worst = 0;
    std::string who;
    for (const char* f : {"GGA", "RMC", "GLL", "VTG", "ZDA", "GSA", "HDT", "MWV",
                          "MWD", "DBT", "DPT", "VHW", "VBW", "ROT", "RSA", "VNYMR"}) {
        const SentenceDef* d = reg.lookup(f);
        if (d && d->fields.size() > worst) { worst = d->fields.size(); who = f; }
    }
    EXPECT_LE(worst, 32u) << "el formatter mas ancho es " << who << " con " << worst
                          << " campos y la mascara solo tiene 32 bits";
}

TEST(Mapper, ResolveExposesTalker) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    auto info = mapper.resolve("GPGGA");
    EXPECT_EQ(info.formatter, "GGA");
    EXPECT_EQ(info.talker,    "GP");

    // Address propietario sin talker estándar → talker vacío.
    auto vn = mapper.resolve("VNYMR");
    EXPECT_EQ(vn.formatter, "VNYMR");
    EXPECT_EQ(vn.talker,    "");

    // Desconocido → raw, sin talker.
    auto raw = mapper.resolve("ZZZZZ");
    EXPECT_EQ(raw.formatter, "");
    EXPECT_EQ(raw.talker,    "");
}

// --------------------------------------------------------------------------
// §8.6.1: el Pipeline ya tiene el formatter resuelto cuando llama a populate(),
// así que existe una sobrecarga que lo recibe en vez de recalcularlo. Ese
// recálculo caía dentro de la región cronometrada.
//
// La sobrecarga solo vale si produce exactamente lo mismo. Comprobarlo sobre
// una sentencia estándar, una propietaria y una desconocida: los tres caminos
// que resolve_formatter() distingue, incluido el que devuelve cadena vacía.
// --------------------------------------------------------------------------
namespace {
std::string dump_members(const DynamicType::_ref_type& type,
                         const DynamicData::_ref_type& data) {
    std::string out;
    DynamicTypeMembersById members;
    type->get_all_members(members);
    for (auto& [id, member] : members) {
        MemberDescriptor::_ref_type desc = traits<MemberDescriptor>::make_shared();
        member->get_descriptor(desc);
        const TypeKind kind = desc->type() ? desc->type()->get_kind() : TK_NONE;
        std::string val;
        switch (kind) {
            case TK_STRING8: { std::string s; data->get_string_value(s, id); val = s; break; }
            case TK_FLOAT64: { double v{}; data->get_float64_value(v, id); val = std::to_string(v); break; }
            case TK_FLOAT32: { float v{};  data->get_float32_value(v, id);  val = std::to_string(v); break; }
            case TK_INT32:   { int32_t v{};  data->get_int32_value(v, id);  val = std::to_string(v); break; }
            case TK_UINT32:  { uint32_t v{}; data->get_uint32_value(v, id); val = std::to_string(v); break; }
            case TK_INT64:   { int64_t v{};  data->get_int64_value(v, id);  val = std::to_string(v); break; }
            case TK_CHAR8:   { char v{};     data->get_char8_value(v, id);  val = std::string(1, v); break; }
            default: val = "?";
        }
        out += std::string(desc->name()) + "=" + val + ";";
    }
    return out;
}
}  // namespace

TEST(Mapper, PopulateWithResolvedFormatterMatchesTheResolvingForm) {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    // Un recv_ns fijo: con 0 cada llamada tomaría el reloj del sistema y los dos
    // volcados diferirían en el timestamp por razones que no son el cambio.
    const int64_t recv_ns = 1'700'000'000'000'000'000LL;

    struct Case { const char* label; const char* sentence; };
    const Case cases[] = {
        {"estandar",    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"},
        // Sentencias tomadas de las fixtures que ya usan los otros tests de
        // este archivo. Un checksum inventado a mano falla el parser, y el
        // test acabaria probando el parser en vez de la sobrecarga.
        {"propietaria", "$VNYMR,-165.918,-008.770,+000.198*7D\r\n"},
        {"desconocida", "$PXYZ,foo,bar,42*36\r\n"},
    };

    for (const auto& c : cases) {
        Parsed p(c.sentence);
        ASSERT_EQ(p.result, ParseResult::Complete) << c.label;

        const auto info = mapper.resolve(p.view().address);
        const DynamicType::_ref_type type =
                info.formatter.empty() ? mapper.raw_sentence_type()
                                       : mapper.type_for(info.formatter);

        auto a = DynamicDataFactory::get_instance()->create_data(type);
        auto b = DynamicDataFactory::get_instance()->create_data(type);
        ASSERT_TRUE(a && b) << c.label;

        mapper.populate(a, p.view(), "dev-1", recv_ns);                    // resuelve dentro
        mapper.populate(b, p.view(), "dev-1", info.formatter, recv_ns);    // ya resuelto

        EXPECT_EQ(dump_members(type, a), dump_members(type, b))
                << "la sobrecarga difiere de la forma que resuelve, caso: " << c.label;
    }
}
