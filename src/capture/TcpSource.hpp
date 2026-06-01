#pragma once

#include <cstdint>
#include <string>

#include "capture/ISource.hpp"

namespace nmea {

// Origen de bytes desde una conexión TCP (NMEA sobre red, D10).
class TcpSource : public ISource {
public:
    ~TcpSource() override { close(); }

    // Conecta a host:port (cliente TCP).
    // Retorna false si la conexión falla.
    bool connect(const std::string& host, uint16_t port, int timeout_ms = 3000) noexcept;

    ssize_t     read(char* buf, std::size_t len, int timeout_ms = 1000) noexcept override;
    bool        is_open()     const noexcept override { return fd_ >= 0; }
    void        close()       noexcept override;
    std::string description() const override { return desc_; }

private:
    int         fd_{-1};
    std::string desc_;
};

}  // namespace nmea
