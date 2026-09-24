// tools/cdr_equivalence_probe.cpp — la compuerta 1, fuera de línea.
//
// Alimenta las mismas sentencias a los dos brazos y compara el CDR bajo la
// definición que la compuerta usa desde 2026-09-23: el **cuerpo** serializado
// más el **identificador de representación**, excluyendo los dos bytes de
// opciones de encapsulación, que Fast CDR calcula y que ninguno de los dos
// brazos elige.
//
// No sustituye a la compuerta de la campaña, que corre 500 sentencias
// capturadas por un suscriptor DDS real. Esto compara la serialización
// directamente, sin red, que es lo que hace falta para saber si el poblado
// generado es correcto antes de refactorizar la ruta de publicación alrededor
// de él. Si aquí falla, allí también, y aquí se diagnostica en segundos.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "NmeaStaticPopulate.hpp"
#include "NmeaDBTPubSubTypes.hpp"
#include "NmeaGGAPubSubTypes.hpp"
#include "NmeaROTPubSubTypes.hpp"
#include "NmeaRSAPubSubTypes.hpp"
#include "NmeaVHWPubSubTypes.hpp"

#include "mapper/Mapper.hpp"
#include "parser/Parser.hpp"
#include "registry/Registry.hpp"

#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>

using namespace nmea;
using namespace eprosima::fastdds::dds;

namespace {

constexpr std::int64_t RECV = 1700000000000000000LL;
int failures = 0;

struct Parsed {
    Parser p;
    ParseResult r{ParseResult::Incomplete};
    explicit Parsed(std::string_view s) {
        for (char c : s) { r = p.consume(c); if (r != ParseResult::Incomplete) break; }
    }
    const SentenceView& v() const { return p.sentence(); }
};

template <typename PST, typename T>
std::vector<unsigned char> ser(PST& pst, T& sample) {
    const std::uint32_t n = pst.calculate_serialized_size(&sample, XCDR2_DATA_REPRESENTATION);
    eprosima::fastdds::rtps::SerializedPayload_t pl(n + 16);
    pst.serialize(&sample, pl, XCDR2_DATA_REPRESENTATION);
    return std::vector<unsigned char>(pl.data, pl.data + pl.length);
}

// La compuerta: identificador de representación idéntico y cuerpo idéntico.
bool gate(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b,
          std::string& why) {
    if (a.size() < 4 || b.size() < 4) { why = "payload mas corto que la cabecera"; return false; }
    if (a[0] != b[0] || a[1] != b[1]) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "identificador de representacion %02X%02X vs %02X%02X",
                      a[0], a[1], b[0], b[1]);
        why = buf; return false;
    }
    if (a.size() != b.size()) {
        why = "tamanos " + std::to_string(a.size()) + " vs " + std::to_string(b.size());
        return false;
    }
    for (std::size_t i = 4; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "cuerpo difiere en el byte %zu: %02X vs %02X",
                          i - 4, a[i], b[i]);
            why = buf; return false;
        }
    }
    return true;
}

template <typename T, typename PST>
void compare(const Mapper& mapper, const char* formatter, const char* label,
             std::string_view sentence) {
    Parsed p(sentence);
    if (p.r != ParseResult::Complete) {
        std::printf("  [FAIL] %-4s %-22s la sentencia no parsea (rc=%d)\n",
                    formatter, label, static_cast<int>(p.r));
        ++failures; return;
    }
    const auto info = mapper.resolve(p.v().address);

    auto dyn_type = mapper.type_for(formatter);
    auto dyn = mapper.map(p.v(), "dev-1", RECV);
    DynamicPubSubType dyn_pst(dyn_type);
    const auto a = ser(dyn_pst, dyn);

    T st;
    populate_static(st, p.v(), "dev-1", info.talker, RECV);
    PST st_pst;
    const auto b = ser(st_pst, st);

    std::string why;
    if (gate(a, b, why)) {
        std::printf("  [ok]   %-4s %-22s %zu bytes\n", formatter, label, a.size());
    } else {
        std::printf("  [FAIL] %-4s %-22s %s\n", formatter, label, why.c_str());
        ++failures;
    }
}

}  // namespace

int main() {
    const Registry reg = Registry::builtin();
    const Mapper mapper(reg);

    std::printf("compuerta 1 fuera de linea: cuerpo CDR + identificador de representacion\n\n");

    // Por formatter: completa, con un campo vacio, y truncada. Los tres casos
    // que la regla de presencia distingue.
    compare<NmeaROT, NmeaROTPubSubType>(mapper, "ROT", "completa",
                                        "$HEROT,-045.7,A*00\r\n");
    compare<NmeaROT, NmeaROTPubSubType>(mapper, "ROT", "campo vacio",
                                        "$HEROT,,A*05\r\n");
    compare<NmeaROT, NmeaROTPubSubType>(mapper, "ROT", "truncada",
                                        "$HEROT,-045.7*6D\r\n");

    compare<NmeaRSA, NmeaRSAPubSubType>(mapper, "RSA", "completa",
                                        "$IIRSA,-012.3,A,+004.5,A*47\r\n");
    compare<NmeaRSA, NmeaRSAPubSubType>(mapper, "RSA", "campos vacios",
                                        "$IIRSA,,V,,V*40\r\n");

    compare<NmeaDBT, NmeaDBTPubSubType>(mapper, "DBT", "completa",
                                        "$SDDBT,036.5,f,011.1,M,006.0,F*01\r\n");
    compare<NmeaDBT, NmeaDBTPubSubType>(mapper, "DBT", "brazas vacias",
                                        "$SDDBT,036.5,f,011.1,M,,F*29\r\n");

    compare<NmeaVHW, NmeaVHWPubSubType>(mapper, "VHW", "completa",
                                        "$VDVHW,045.0,T,043.2,M,010.5,N,019.4,K*4B\r\n");

    compare<NmeaGGA, NmeaGGAPubSubType>(mapper, "GGA", "dgps_age vacio",
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");

    std::printf("\n%d comparacion(es) fallida(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
