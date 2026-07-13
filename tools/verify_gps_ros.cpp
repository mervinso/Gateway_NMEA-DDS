// tools/verify_gps_ros.cpp — validación end-to-end GPS→ROS (NavSatFix) sin GUI.
// Reproduce lo que GatewayController::connectInterface + enableRos construyen:
// UdpSource → Pipeline (on_sentence) → RosPublisher, publicando /gps/fix.
// Verificación externa: `ros2 topic echo /gps/fix` debe mostrar lat/lon en
// grados decimales con signo (W ⇒ longitud negativa).
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "capture/UdpSource.hpp"
#include "pipeline/Pipeline.hpp"
#include "registry/Registry.hpp"
#include "ros/RosPublisher.hpp"

using namespace nmea;

static std::atomic<bool> g_stop{false};

int main(int argc, char** argv) {
    const uint16_t port = (argc > 1) ? static_cast<uint16_t>(std::atoi(argv[1])) : 3100;
    const int run_secs  = (argc > 2) ? std::atoi(argv[2]) : 15;

    std::signal(SIGINT, [](int) { g_stop.store(true); });

    Registry registry = Registry::builtin();

    ros::RosPublisher ros_pub;
    ros_pub.enable("GP", "GLL", {ros::RosTarget::NavSatFix, "/gps/fix", "gps"});

    auto src = std::make_unique<UdpSource>();
    if (!src->open(port)) {
        std::cerr << "No se pudo abrir udp://" << port << "\n";
        return 1;
    }

    std::atomic<int> forwarded{0};
    Pipeline::Config cfg;
    cfg.source         = std::move(src);
    cfg.registry       = &registry;
    cfg.publish_to_dds = false;  // solo validamos el camino ROS
    cfg.on_sentence = [&](std::string talker, std::string formatter,
                          std::string category,
                          std::vector<std::string> names,
                          std::vector<std::string> values) {
        ros_pub.onSentence(talker, formatter, category, names, values);
        if (talker == "GP" && formatter == "GLL") {
            ++forwarded;
            std::cout << "GLL → /gps/fix";
            for (size_t i = 0; i < names.size() && i < values.size(); ++i)
                if (names[i] == "latitude" || names[i] == "ns_indicator" ||
                    names[i] == "longitude" || names[i] == "ew_indicator")
                    std::cout << "  " << names[i] << "=" << values[i];
            std::cout << "\n";
        }
    };

    Pipeline pipeline(std::move(cfg));
    pipeline.start();
    std::cout << "Escuchando udp://" << port << " durante " << run_secs
              << " s; publicando GP/GLL como NavSatFix en /gps/fix\n";

    for (int i = 0; i < run_secs * 10 && !g_stop.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    pipeline.stop();
    std::cout << "Sentencias GLL reenviadas a ROS: " << forwarded.load() << "\n";
    return forwarded.load() > 0 ? 0 : 2;
}
