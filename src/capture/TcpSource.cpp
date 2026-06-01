#include "capture/TcpSource.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace nmea {

void TcpSource::close() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool TcpSource::connect(const std::string& host, uint16_t port, int timeout_ms) noexcept {
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const std::string port_str = std::to_string(port);
    struct addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) return false;

    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype | SOCK_CLOEXEC, p->ai_protocol);
        if (fd < 0) continue;

        // Non-blocking connect con timeout via select().
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        const int r = ::connect(fd, p->ai_addr, p->ai_addrlen);
        if (r == 0 || errno == EINPROGRESS) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            struct timeval tv{};
            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000L;
            if (select(fd + 1, nullptr, &wfds, nullptr, &tv) > 0) {
                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0) {
                    // Vuelve a modo bloqueante; usamos select en read().
                    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
                    break;
                }
            }
        }
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) return false;

    close();
    fd_   = fd;
    desc_ = "tcp://" + host + ":" + port_str;
    return true;
}

ssize_t TcpSource::read(char* buf, std::size_t len, int timeout_ms) noexcept {
    if (fd_ < 0) return -1;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);

    struct timeval tv{};
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000L;

    const int ret = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <  0) return -1;
    if (ret == 0) return  0;

    const ssize_t n = ::read(fd_, buf, len);
    return (n == 0) ? -1 : n;  // TCP EOF = desconexión → -1
}

}  // namespace nmea
