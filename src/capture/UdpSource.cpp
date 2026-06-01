#include "capture/UdpSource.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nmea {

void UdpSource::close() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UdpSource::open(uint16_t port) noexcept {
    const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    // SO_REUSEADDR/REUSEPORT permiten coexistir con otros receptores del broadcast.
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return false;
    }

    close();
    fd_   = fd;
    desc_ = "udp://" + std::to_string(port);
    return true;
}

ssize_t UdpSource::read(char* buf, std::size_t len, int timeout_ms) noexcept {
    if (fd_ < 0) return -1;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);

    struct timeval tv{};
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000L;

    const int ret = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <  0) return -1;  // error real
    if (ret == 0) return  0;  // timeout sin datos

    // UDP no tiene EOF: un datagrama de 0 bytes es válido (no es desconexión).
    const ssize_t n = recvfrom(fd_, buf, len, 0, nullptr, nullptr);
    return (n < 0) ? -1 : n;
}

}  // namespace nmea
