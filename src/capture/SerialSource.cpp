#include "capture/SerialSource.hpp"

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace nmea {

namespace {
// Mapea baud int a la constante POSIX correspondiente.
speed_t to_speed(int baud) noexcept {
    switch (baud) {
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B0;
    }
}
}  // namespace

bool SerialSource::open(const std::string& path, int baud) noexcept {
    const speed_t speed = to_speed(baud);
    if (speed == B0) return false;

    // Abre con O_NONBLOCK para no bloquearse en carrier-detect;
    // después lo limpiamos y usamos select() para el timeout en read().
    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;

    // Raw mode 8N1 + baud.
    struct termios tty{};
    if (tcgetattr(fd, &tty) < 0) { ::close(fd); return false; }
    cfmakeraw(&tty);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tty) < 0) { ::close(fd); return false; }

    // Retira O_NONBLOCK: usamos select() para el timeout, no reads no-bloqueantes.
    const int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    close();  // cierra el anterior si había uno abierto
    fd_   = fd;
    path_ = path;
    return true;
}

void SerialSource::close() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

ssize_t SerialSource::read(char* buf, std::size_t len, int timeout_ms) noexcept {
    if (fd_ < 0) return -1;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);

    struct timeval tv{};
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000L;

    const int ret = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <  0) return -1;  // error
    if (ret == 0) return  0;  // timeout

    const ssize_t n = ::read(fd_, buf, len);
    return (n == 0) ? -1 : n;  // 0 = EOF/cierre del otro extremo → error
}

}  // namespace nmea
