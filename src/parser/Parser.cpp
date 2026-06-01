#include "parser/Parser.hpp"

namespace nmea {

namespace {
// Convierte un dígito hexadecimal a su valor; -1 si no es hex.
constexpr int hexNibble(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
}  // namespace

void Parser::reset() noexcept {
    len_ = 0;
    token_count_ = 0;
    cur_token_start_ = 0;
    checksum_ = 0;
    expected_ = 0;
}

ParseResult Parser::finalize() noexcept {
    // tokens_[0] = dirección; tokens_[1..token_count_) = campos.
    view_.address = std::string_view(buf_.data() + tokens_[0].start, tokens_[0].len);
    const std::size_t nfields = token_count_ - 1;
    for (std::size_t i = 0; i < nfields; ++i) {
        field_views_[i] =
            std::string_view(buf_.data() + tokens_[i + 1].start, tokens_[i + 1].len);
    }
    view_.fields = std::span<const std::string_view>(field_views_.data(), nfields);
    state_ = State::WaitStart;
    return (checksum_ == expected_) ? ParseResult::Complete : ParseResult::ChecksumError;
}

ParseResult Parser::consume(char byte) noexcept {
    // '$' (paramétricas) y '!' (encapsulación AIS VDM/VDO) inician/resincronizan
    // el frame. NMEA 0183 admite ambos delimitadores; el checksum XOR excluye el
    // delimitador en los dos casos (igual lógica, ya que reset() empieza tras él).
    if (byte == '$' || byte == '!') {
        reset();
        state_ = State::Body;
        return ParseResult::Incomplete;
    }

    switch (state_) {
        case State::WaitStart:
            return ParseResult::Incomplete;

        case State::Body: {
            if (byte == '\r' || byte == '\n') {
                state_ = State::WaitStart;  // terminador sin checksum
                return ParseResult::Framing;
            }
            if (byte == '*') {  // cierra la dirección/último campo, empieza checksum
                if (token_count_ > kMaxFields) {
                    state_ = State::WaitStart;
                    return ParseResult::Overflow;
                }
                tokens_[token_count_++] = {static_cast<std::uint16_t>(cur_token_start_),
                                           static_cast<std::uint16_t>(len_ - cur_token_start_)};
                state_ = State::Csum1;
                return ParseResult::Incomplete;
            }
            if (byte == ',') {  // separador de campo (sí cuenta para el checksum)
                checksum_ ^= static_cast<std::uint8_t>(byte);
                if (token_count_ > kMaxFields) {
                    state_ = State::WaitStart;
                    return ParseResult::Overflow;
                }
                tokens_[token_count_++] = {static_cast<std::uint16_t>(cur_token_start_),
                                           static_cast<std::uint16_t>(len_ - cur_token_start_)};
                cur_token_start_ = len_;
                return ParseResult::Incomplete;
            }
            if (len_ >= kMaxSentence) {
                state_ = State::WaitStart;
                return ParseResult::Overflow;
            }
            checksum_ ^= static_cast<std::uint8_t>(byte);
            buf_[len_++] = byte;
            return ParseResult::Incomplete;
        }

        case State::Csum1: {
            const int n = hexNibble(byte);
            if (n < 0) {
                state_ = State::WaitStart;
                return ParseResult::Framing;
            }
            expected_ = static_cast<std::uint8_t>(n << 4);
            state_ = State::Csum2;
            return ParseResult::Incomplete;
        }

        case State::Csum2: {
            const int n = hexNibble(byte);
            if (n < 0) {
                state_ = State::WaitStart;
                return ParseResult::Framing;
            }
            expected_ |= static_cast<std::uint8_t>(n);
            return finalize();
        }
    }
    return ParseResult::Incomplete;
}

}  // namespace nmea
