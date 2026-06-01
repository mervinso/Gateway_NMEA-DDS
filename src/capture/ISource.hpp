#pragma once

#include <cstddef>
#include <string>
#include <sys/types.h>

namespace nmea {

// Abstracción de origen de bytes para el pipeline de captura (D13).
// Alimenta el bucle: read() → parser FSM → mapper → publish.
class ISource {
public:
    virtual ~ISource() = default;

    // Lee hasta `len` bytes en `buf` con un timeout.
    // Retorna: >0 bytes leídos, 0 timeout sin datos, -1 error/desconexión.
    virtual ssize_t read(char* buf, std::size_t len, int timeout_ms = 1000) noexcept = 0;

    virtual bool        is_open()     const noexcept = 0;
    virtual void        close()       noexcept = 0;
    virtual std::string description() const = 0;
};

}  // namespace nmea
