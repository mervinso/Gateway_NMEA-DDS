#pragma once

#include <string>

#include "capture/ISource.hpp"

namespace nmea {

// Origen de bytes desde un puerto serie POSIX/termios (hot path, sin Qt).
// Qt se usa solo para enumerar puertos (cold path, fuera de esta clase).
class SerialSource : public ISource {
public:
    ~SerialSource() override { close(); }

    // Abre el puerto en modo raw 8N1 al baud indicado.
    // Retorna false si el dispositivo no existe o no se puede abrir.
    bool open(const std::string& path, int baud) noexcept;

    ssize_t     read(char* buf, std::size_t len, int timeout_ms = 1000) noexcept override;
    bool        is_open()     const noexcept override { return fd_ >= 0; }
    void        close()       noexcept override;
    std::string description() const override { return path_; }

private:
    int         fd_{-1};
    std::string path_;
};

}  // namespace nmea
