#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>

#include "capture/BaudDetector.hpp"
#include "capture/SerialSource.hpp"
#include "capture/TcpSource.hpp"

using namespace nmea;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Par pty usando posix_openpt(): sin socat, sin subprocesos.
struct PtyPair {
    int         master{-1};
    std::string slave_path;

    PtyPair() {
        master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (master < 0) return;
        if (grantpt(master) < 0 || unlockpt(master) < 0) {
            ::close(master);
            master = -1;
            return;
        }
        slave_path = ptsname(master);
    }
    ~PtyPair() { if (master >= 0) ::close(master); }

    bool   valid()    const { return master >= 0; }
    ssize_t write(std::string_view sv) const {
        return ::write(master, sv.data(), sv.size());
    }
};

// Servidor TCP local para los tests de TcpSource.
// Escucha en loopback, acepta una conexión, envía datos y cierra.
struct LocalTcpServer {
    int      listen_fd{-1};
    uint16_t port{0};
    std::thread thread;
    std::atomic<bool> ready{false};

    LocalTcpServer() {
        listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listen_fd < 0) return;
        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;  // puerto asignado por el SO
        if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return;
        if (listen(listen_fd, 1) < 0) return;
        socklen_t len = sizeof(addr);
        getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
        port = ntohs(addr.sin_port);
    }

    void serve(std::string_view payload) {
        ready.store(true);
        int client = accept(listen_fd, nullptr, nullptr);
        if (client < 0) return;
        ::write(client, payload.data(), payload.size());
        ::close(client);
    }

    void start(std::string_view payload) {
        thread = std::thread([this, p = std::string(payload)]{ serve(p); });
        // espera a que el servidor esté listo
        while (!ready.load()) std::this_thread::yield();
    }

    ~LocalTcpServer() {
        if (listen_fd >= 0) ::close(listen_fd);
        if (thread.joinable()) thread.join();
    }

    bool valid() const { return listen_fd >= 0 && port > 0; }
};

// ---------------------------------------------------------------------------
// SerialSource tests
// ---------------------------------------------------------------------------

TEST(SerialSource, OpensVirtualPtyAndIsOpen) {
    PtyPair pty;
    ASSERT_TRUE(pty.valid()) << "posix_openpt falló (entorno sin pty?)";

    SerialSource src;
    EXPECT_TRUE(src.open(pty.slave_path, 115200));
    EXPECT_TRUE(src.is_open());
    EXPECT_EQ(src.description(), pty.slave_path);
}

TEST(SerialSource, ReadDeliversBytesWrittenToMaster) {
    PtyPair pty;
    ASSERT_TRUE(pty.valid());

    SerialSource src;
    ASSERT_TRUE(src.open(pty.slave_path, 115200));

    constexpr std::string_view sentence =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    ASSERT_GT(pty.write(sentence), 0);

    char buf[128]{};
    const ssize_t n = src.read(buf, sizeof(buf), 500);

    ASSERT_GT(n, 0);
    EXPECT_EQ(std::string_view(buf, 6), "$GPGGA");
}

TEST(SerialSource, OpenFailsForNonexistentPath) {
    SerialSource src;
    EXPECT_FALSE(src.open("/dev/ttyNONEXISTENT_GW_TEST", 9600));
    EXPECT_FALSE(src.is_open());
}

TEST(SerialSource, ReadTimeoutReturnsZero) {
    PtyPair pty;
    ASSERT_TRUE(pty.valid());

    SerialSource src;
    ASSERT_TRUE(src.open(pty.slave_path, 115200));

    // No escribimos nada → timeout.
    char buf[16]{};
    const ssize_t n = src.read(buf, sizeof(buf), 50);  // 50 ms timeout
    EXPECT_EQ(n, 0);
}

TEST(SerialSource, CloseReleasesPort) {
    PtyPair pty;
    ASSERT_TRUE(pty.valid());

    SerialSource src;
    ASSERT_TRUE(src.open(pty.slave_path, 115200));
    src.close();
    EXPECT_FALSE(src.is_open());
}

// ---------------------------------------------------------------------------
// TcpSource tests
// ---------------------------------------------------------------------------

TEST(TcpSource, ConnectsAndReadsFromLoopback) {
    constexpr std::string_view payload =
        "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48\r\n";

    LocalTcpServer server;
    ASSERT_TRUE(server.valid());
    server.start(payload);

    TcpSource src;
    ASSERT_TRUE(src.connect("127.0.0.1", server.port, 1000));
    EXPECT_TRUE(src.is_open());
    EXPECT_NE(src.description().find("tcp://"), std::string::npos);

    char buf[128]{};
    const ssize_t n = src.read(buf, sizeof(buf), 1000);

    ASSERT_GT(n, 0);
    EXPECT_EQ(std::string_view(buf, 6), "$GPVTG");
}

TEST(TcpSource, ConnectFailsOnClosedPort) {
    TcpSource src;
    // Puerto 1 está siempre cerrado sin privilegios.
    EXPECT_FALSE(src.connect("127.0.0.1", 1, 200));
    EXPECT_FALSE(src.is_open());
}

// ---------------------------------------------------------------------------
// BaudDetector test
// ---------------------------------------------------------------------------

TEST(BaudDetector, DetectsBaudWhenValidNmeaPresent) {
    PtyPair pty;
    ASSERT_TRUE(pty.valid());

    // Hilo escritor: envía un stream de GGAs al maestro para que BaudDetector las cuente.
    std::atomic<bool> stop{false};
    std::thread writer([&] {
        constexpr std::string_view gga =
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
        while (!stop.load()) {
            if (::write(pty.master, gga.data(), gga.size()) < 0) break;
            usleep(10'000);  // 10 ms entre sentencias
        }
    });

    // BaudDetector intenta todos los candidatos. En un pty el baud es ignorado
    // por el kernel, así que el primer candidato con suficientes checksums gana.
    const int detected = BaudDetector::detect(pty.slave_path, /*sample_ms=*/300);
    stop.store(true);
    writer.join();

    EXPECT_NE(detected, 0) << "BaudDetector no detectó NMEA válido";
}
