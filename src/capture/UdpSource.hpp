#pragma once

#include <cstdint>
#include <string>

#include "capture/ISource.hpp"

namespace nmea {

// Origen de bytes desde un socket UDP (NMEA por broadcast/unicast UDP, D10).
// Hace bind a un puerto local y recibe datagramas (típicamente un sentence por
// datagrama). Sin semántica de EOF: UDP no es orientado a conexión.
class UdpSource : public ISource {
public:
    ~UdpSource() override { close(); }

    // Hace bind a 0.0.0.0:port para recibir datagramas (incluye broadcast).
    // Retorna false si el socket o el bind fallan.
    bool open(uint16_t port) noexcept;

    ssize_t     read(char* buf, std::size_t len, int timeout_ms = 1000) noexcept override;
    bool        is_open()     const noexcept override { return fd_ >= 0; }
    void        close()       noexcept override;
    std::string description() const override { return desc_; }

private:
    int         fd_{-1};
    std::string desc_;
};

}  // namespace nmea
