#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace nmea {

// Resultado de alimentar un byte a la FSM del parser.
enum class ParseResult {
    Incomplete,     // sentencia aún en curso
    Complete,       // sentencia completa y con checksum válido (ver sentence())
    ChecksumError,  // se completó el frame pero el checksum no coincide
    Overflow,       // la sentencia excede la capacidad estática
    Framing,        // ruptura de framing (CR/LF inesperado, etc.)
};

// Vista de solo-lectura sobre una sentencia completa. Las vistas apuntan al
// buffer interno del Parser y son válidas hasta el siguiente consume().
struct SentenceView {
    std::string_view address;                  // token antes de la 1ª coma: "GPGGA", "VNYMR"
    std::span<const std::string_view> fields;  // campos de datos tras la dirección
};

// Parser NMEA freestanding: framing + validación de checksum + tokenización.
// Sin asignación dinámica, sin std::string, sin excepciones. La interpretación
// semántica de campos NO es responsabilidad de esta clase (ver CONTEXT.md).
class Parser {
public:
    static constexpr std::size_t kMaxSentence = 128;  // bytes entre '$' y '*'
    static constexpr std::size_t kMaxFields = 32;

    // Alimenta un byte. Devuelve Complete cuando hay una sentencia válida lista.
    ParseResult consume(char byte) noexcept;

    // Vista de la última sentencia completa. Válida tras recibir Complete.
    const SentenceView& sentence() const noexcept { return view_; }

private:
    enum class State : std::uint8_t { WaitStart, Body, Csum1, Csum2 };
    struct Token {
        std::uint16_t start;
        std::uint16_t len;
    };

    void reset() noexcept;
    ParseResult finalize() noexcept;

    State state_{State::WaitStart};
    std::array<char, kMaxSentence> buf_{};        // bytes entre '$' y '*' (sin comas)
    std::size_t len_{0};
    std::array<Token, kMaxFields + 1> tokens_{};  // [0] = dirección; [1..] = campos
    std::size_t token_count_{0};
    std::size_t cur_token_start_{0};
    std::uint8_t checksum_{0};                     // XOR acumulado
    std::uint8_t expected_{0};                     // checksum parseado de la sentencia
    std::array<std::string_view, kMaxFields> field_views_{};
    SentenceView view_{};
};

}  // namespace nmea
