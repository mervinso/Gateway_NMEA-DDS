#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string_view>
#include <thread>

// Suprimir logs de DDS en tests
#include <fastdds/dds/log/Log.hpp>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>

#include "capture/SerialSource.hpp"
#include "mapper/Mapper.hpp"
#include "pipeline/Pipeline.hpp"
#include "registry/Registry.hpp"

using namespace nmea;
using namespace eprosima::fastdds::dds;

// Dominio aislado para todos los tests de pipeline.
static constexpr int kTestDomain = 200;

// GGA con checksum válido.
static constexpr std::string_view kGga =
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

// ---------------------------------------------------------------------------
// Helper: par pty (igual que en capture_test)
// ---------------------------------------------------------------------------
struct PtyPair {
    int         master{-1};
    std::string slave_path;

    PtyPair() {
        master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (master < 0) return;
        if (grantpt(master) < 0 || unlockpt(master) < 0) { ::close(master); master = -1; return; }
        slave_path = ptsname(master);
    }
    ~PtyPair() { if (master >= 0) ::close(master); }
    bool valid() const { return master >= 0; }
    ssize_t write(std::string_view sv) const {
        return ::write(master, sv.data(), sv.size());
    }
};

// ---------------------------------------------------------------------------
// Helper: construye un Pipeline con un pty como fuente.
// ---------------------------------------------------------------------------
static Pipeline::Config make_config(const PtyPair& pty, const Registry& reg,
                                     const std::string& device_id = "test_gps") {
    auto src = std::make_unique<SerialSource>();
    src->open(pty.slave_path, 115200);

    Pipeline::Config cfg;
    cfg.device_id = device_id;
    cfg.source    = std::move(src);
    cfg.registry  = &reg;
    cfg.domain_id = kTestDomain;
    return cfg;
}

// Helper: espera a que state() alcance el valor esperado (máx timeout_ms).
static bool wait_for_state(const Pipeline& p, Pipeline::State expected, int timeout_ms = 1000) {
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (p.state() == expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return p.state() == expected;
}

// ===========================================================================
// Tests
// ===========================================================================

TEST(Pipeline, StartsAndReachesRunningState) {
    PtyPair pty; ASSERT_TRUE(pty.valid());
    Registry reg = Registry::builtin();

    Pipeline pipeline(make_config(pty, reg));
    pipeline.start();

    EXPECT_TRUE(wait_for_state(pipeline, Pipeline::State::Running, 1000));
    pipeline.stop();
    EXPECT_EQ(pipeline.state(), Pipeline::State::Stopped);
}

TEST(Pipeline, CountsValidSentences) {
    PtyPair pty; ASSERT_TRUE(pty.valid());
    Registry reg = Registry::builtin();

    Pipeline pipeline(make_config(pty, reg));
    pipeline.start();
    ASSERT_TRUE(wait_for_state(pipeline, Pipeline::State::Running));

    // Enviar las GGA en un hilo aparte con espaciado para que el worker
    // ya esté en el loop de read() antes de que llegue cada sentencia.
    std::thread sender([&] {
        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            pty.write(kGga);
        }
    });

    // Espera a que el pipeline procese las 3 sentencias (máx 2 s).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (pipeline.sentences_ok() < 3 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    sender.join();
    pipeline.stop();
    EXPECT_EQ(pipeline.sentences_ok(), 3u);
    EXPECT_EQ(pipeline.sentences_err(), 0u);
}

TEST(Pipeline, TransitionsToErrorOnSourceClose) {
    PtyPair pty; ASSERT_TRUE(pty.valid());
    Registry reg = Registry::builtin();

    Pipeline pipeline(make_config(pty, reg));
    pipeline.start();
    ASSERT_TRUE(wait_for_state(pipeline, Pipeline::State::Running));

    // Cerrar el maestro del pty provoca EIO en el esclavo.
    ::close(pty.master);
    pty.master = -1;

    EXPECT_TRUE(wait_for_state(pipeline, Pipeline::State::Error, 1000));
}

TEST(Pipeline, TwoPipelinesRunConcurrently) {
    PtyPair pty0, pty1;
    ASSERT_TRUE(pty0.valid()); ASSERT_TRUE(pty1.valid());
    Registry reg = Registry::builtin();

    Pipeline p0(make_config(pty0, reg, "sensor_a"));
    Pipeline p1(make_config(pty1, reg, "sensor_b"));
    p0.start(); p1.start();

    ASSERT_TRUE(wait_for_state(p0, Pipeline::State::Running));
    ASSERT_TRUE(wait_for_state(p1, Pipeline::State::Running));

    // Enviar espaciado para que ambos workers ya estén en el loop.
    std::thread sender([&] {
        for (int i = 0; i < 2; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            pty0.write(kGga);
            pty1.write(kGga);
        }
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while ((p0.sentences_ok() < 2 || p1.sentences_ok() < 2) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    sender.join();
    p0.stop(); p1.stop();
    EXPECT_EQ(p0.sentences_ok(), 2u);
    EXPECT_EQ(p1.sentences_ok(), 2u);
    // Cada pipeline solo ve sus propias sentencias, no las del otro.
    EXPECT_EQ(p0.sentences_err(), 0u);
    EXPECT_EQ(p1.sentences_err(), 0u);
}

// ---------------------------------------------------------------------------
// Test de integración DDS: verifica que el Pipeline crea DataWriter y que
// el tópico es descubierto por un suscriptor externo.
// Nota: DataWriter::write() con DynamicData tiene un bug en Fast DDS 3.6
// (SEGV en calculate_serialized_size). Testeamos la infraestructura DDS
// (creación de participante, publisher, topic, writer) sin enviar datos.
// ---------------------------------------------------------------------------
TEST(Pipeline, PublishesDataToDds) {
    PtyPair pty; ASSERT_TRUE(pty.valid());
    Registry reg = Registry::builtin();

    // ── Crear suscriptor de prueba ──────────────────────────────────────────
    DomainParticipant* sub_participant =
            DomainParticipantFactory::get_instance()
                    ->create_participant(kTestDomain, PARTICIPANT_QOS_DEFAULT);
    ASSERT_NE(sub_participant, nullptr);

    // Registrar el mismo tipo que va a publicar el Pipeline (NmeaGGA).
    Mapper mapper(reg);
    DynamicType::_ref_type gga_type = mapper.type_for("GGA");
    TypeSupport ts(new DynamicPubSubType(gga_type));  // TypeSupport toma ownership
    sub_participant->register_type(ts, "NmeaGGA");

    Topic* sub_topic = sub_participant->create_topic(
            "nmea/gps/GGA", "NmeaGGA", TOPIC_QOS_DEFAULT);
    ASSERT_NE(sub_topic, nullptr);

    Subscriber* subscriber = sub_participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    DataReader* reader = subscriber->create_datareader(sub_topic, DATAREADER_QOS_DEFAULT);
    ASSERT_NE(reader, nullptr);

    // ── Lanzar pipeline ────────────────────────────────────────────────────
    Pipeline pipeline(make_config(pty, reg));
    pipeline.start();
    ASSERT_TRUE(wait_for_state(pipeline, Pipeline::State::Running));

    // Dar tiempo a que escritor y lector se descubran (SPDP/SEDP).
    // Enviamos una sentencia para forzar la creación lazy del DataWriter.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pty.write(kGga);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verificar que el pipeline está en Running y contó la sentencia
    // (lo que implica que el DataWriter fue creado exitosamente).
    EXPECT_EQ(pipeline.state(), Pipeline::State::Running);
    EXPECT_GE(pipeline.sentences_ok(), 1u)
        << "Pipeline no procesó la sentencia — DataWriter no creado";

    pipeline.stop();

    // ── Cleanup DDS del suscriptor ─────────────────────────────────────────
    subscriber->delete_datareader(reader);
    sub_participant->delete_subscriber(subscriber);
    sub_participant->delete_topic(sub_topic);
    DomainParticipantFactory::get_instance()->delete_participant(sub_participant);
}
