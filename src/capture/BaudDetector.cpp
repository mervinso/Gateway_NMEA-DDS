#include "capture/BaudDetector.hpp"

#include <chrono>

#include "capture/SerialSource.hpp"
#include "parser/Parser.hpp"

namespace nmea {

int BaudDetector::detect(const std::string& path, int sample_ms, int min_valid) noexcept {
    int best_baud  = 0;
    int best_valid = 0;

    for (int baud : kCandidates()) {
        SerialSource src;
        if (!src.open(path, baud)) continue;

        Parser parser;
        int valid = 0;

        const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(sample_ms);

        while (std::chrono::steady_clock::now() < deadline) {
            const int remaining = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now()).count());
            if (remaining <= 0) break;

            char buf[64];
            const ssize_t n = src.read(buf, sizeof(buf), remaining);
            if (n < 0) break;

            for (ssize_t i = 0; i < n; ++i) {
                if (parser.consume(buf[i]) == ParseResult::Complete)
                    ++valid;
            }
        }

        if (valid > best_valid) {
            best_valid = valid;
            best_baud  = baud;
        }
    }

    return (best_valid >= min_valid) ? best_baud : 0;
}

}  // namespace nmea
