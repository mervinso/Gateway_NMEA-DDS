#pragma once

#include <string>
#include <vector>

namespace nmea {

// Detecta el baud rate de un puerto serie abriendo a cada candidato,
// leyendo durante sample_ms ms, y contando sentencias NMEA con checksum válido.
// Retorna el baud ganador o 0 si ninguno supera el umbral mínimo.
class BaudDetector {
public:
    static constexpr std::vector<int> kCandidates() {
        return {4800, 9600, 38400, 57600, 115200};
    }

    // `min_valid`: mínimo de sentencias correctas para considerar un baud ganador.
    static int detect(const std::string& path,
                      int sample_ms = 500,
                      int min_valid = 1) noexcept;
};

}  // namespace nmea
