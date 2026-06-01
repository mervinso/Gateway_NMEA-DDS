#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <string>
#include <string_view>

#include "parser/Parser.hpp"

using namespace nmea;

// Contador global de asignaciones para verificar el camino caliente sin malloc.
namespace {
std::atomic<long> g_allocations{0};
}
void* operator new(std::size_t n) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(n ? n : 1);
}
void* operator new[](std::size_t n) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(n ? n : 1);
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
// Alimenta una cadena byte a byte; se detiene en el primer resultado terminal
// (uso realista: la sentencia se procesa en cuanto se resuelve).
ParseResult feed(Parser& p, std::string_view s) {
    for (char c : s) {
        const ParseResult r = p.consume(c);
        if (r != ParseResult::Incomplete) return r;
    }
    return ParseResult::Incomplete;
}
}  // namespace

TEST(Parser, ParsesValidGgaSentence) {
    Parser p;
    // Ejemplo GGA canónico con checksum válido (*47).
    constexpr std::string_view gga =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    const ParseResult r = feed(p, gga);

    EXPECT_EQ(r, ParseResult::Complete);
    EXPECT_EQ(p.sentence().address, "GPGGA");
}

TEST(Parser, SplitsFieldsIncludingTrailingEmpties) {
    Parser p;
    constexpr std::string_view gga =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    ASSERT_EQ(feed(p, gga), ParseResult::Complete);

    const auto& s = p.sentence();
    ASSERT_EQ(s.fields.size(), 14u);
    EXPECT_EQ(s.fields[0], "123519");
    EXPECT_EQ(s.fields[1], "4807.038");
    EXPECT_EQ(s.fields[2], "N");
    EXPECT_EQ(s.fields[10], "46.9");
    EXPECT_EQ(s.fields[11], "M");
    EXPECT_EQ(s.fields[12], "");  // campo vacío preservado
    EXPECT_EQ(s.fields[13], "");  // campo vacío final preservado
}

TEST(Parser, RejectsBadChecksum) {
    Parser p;
    // Misma GGA con checksum incorrecto (*00 en vez de *47).
    constexpr std::string_view bad =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00\r\n";

    EXPECT_EQ(feed(p, bad), ParseResult::ChecksumError);
}

TEST(Parser, ParsesProprietaryVectorNavSentence) {
    Parser p;
    constexpr std::string_view vn = "$VNYMR,-165.918,-008.770,+000.198*7D\r\n";

    ASSERT_EQ(feed(p, vn), ParseResult::Complete);
    EXPECT_EQ(p.sentence().address, "VNYMR");  // token único, sin split talker/formatter
    ASSERT_EQ(p.sentence().fields.size(), 3u);
    EXPECT_EQ(p.sentence().fields[0], "-165.918");
}

TEST(Parser, ResyncsOnNewStartDelimiter) {
    Parser p;
    // Fragmento truncado (sin terminar) seguido de una sentencia válida.
    constexpr std::string_view stream =
        "$GPGG"  // basura/truncado: el siguiente '$' debe reiniciar
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    ASSERT_EQ(feed(p, stream), ParseResult::Complete);
    EXPECT_EQ(p.sentence().address, "GPGGA");
}

TEST(Parser, ConsumeDoesNotAllocate) {
    Parser p;
    constexpr std::string_view gga =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

    // Mido estrictamente alrededor del bucle de parseo (las macros de gtest solo
    // asignan al fallar, no al pasar).
    const long before = g_allocations.load(std::memory_order_relaxed);
    ParseResult r = ParseResult::Incomplete;
    for (char c : gga) {
        r = p.consume(c);
        if (r != ParseResult::Incomplete) break;
    }
    const long after = g_allocations.load(std::memory_order_relaxed);

    EXPECT_EQ(r, ParseResult::Complete);
    EXPECT_EQ(after - before, 0) << "consume() asignó memoria dinámica";
}

TEST(Parser, RejectsSentenceWithoutChecksum) {
    Parser p;
    // VTG sin checksum: termina en CR/LF dentro del cuerpo → política: rechazar.
    constexpr std::string_view no_csum = "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K\r\n";

    EXPECT_EQ(feed(p, no_csum), ParseResult::Framing);
}

TEST(Parser, OverflowsThenRecovers) {
    Parser p;
    // Cuerpo más largo que la capacidad estática, sin terminar.
    std::string oversized = "$";
    oversized.append(Parser::kMaxSentence + 8, 'A');
    EXPECT_EQ(feed(p, oversized), ParseResult::Overflow);

    // Tras el overflow, una sentencia válida se vuelve a parsear (recuperación).
    constexpr std::string_view gga =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    ASSERT_EQ(feed(p, gga), ParseResult::Complete);
    EXPECT_EQ(p.sentence().address, "GPGGA");
}

TEST(Parser, ParsesAisEncapsulationSentenceWithBangDelimiter) {
    Parser p;
    // Sentencia de encapsulación AIS (delimitador '!'), capturada del simulador.
    // NMEA 0183 admite '!' como inicio de frame igual que '$' (mismo checksum XOR).
    constexpr std::string_view vdm =
        "!AIVDM,1,1,,A,17PaewhP2HrEre646EiEm4cl0000,0*6A\r\n";

    ASSERT_EQ(feed(p, vdm), ParseResult::Complete);
    EXPECT_EQ(p.sentence().address, "AIVDM");
    ASSERT_EQ(p.sentence().fields.size(), 6u);
    EXPECT_EQ(p.sentence().fields[0], "1");                              // total
    EXPECT_EQ(p.sentence().fields[4], "17PaewhP2HrEre646EiEm4cl0000");   // payload
}

TEST(Parser, ResyncsOnBangAfterGarbage) {
    Parser p;
    // Un '!' a mitad de ruido debe reiniciar el frame igual que '$'.
    constexpr std::string_view stream =
        "garbage!AIVDO,1,1,,A,17PaewhP2HrEre646EiEm4cl0000,0*68\r\n";
    ASSERT_EQ(feed(p, stream), ParseResult::Complete);
    EXPECT_EQ(p.sentence().address, "AIVDO");
}
