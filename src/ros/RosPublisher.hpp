// src/ros/RosPublisher.hpp
#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <builtin_interfaces/msg/time.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <rclcpp/rclcpp.hpp>

namespace nmea::ros {

// Destino ROS de una conversión: tipo de mensaje, tópico y frame_id del header.
struct RosTarget {
    enum Type { Imu, NavSatFix } type;
    std::string topic;     // p.ej. "/imu/data"
    std::string frame_id;  // p.ej. "imu_link"
};

// ── Funciones puras (sin estado ROS), unit-testables sin spin ────────────────
sensor_msgs::msg::Imu imuFromFields(
    const std::vector<std::string>& names,
    const std::vector<std::string>& values,
    const std::string& frame_id,
    const builtin_interfaces::msg::Time& stamp);

sensor_msgs::msg::NavSatFix navSatFixFromFields(
    const std::vector<std::string>& names,
    const std::vector<std::string>& values,
    const std::string& frame_id,
    const builtin_interfaces::msg::Time& stamp);

// Nodo ROS embebido (sin spin) que publica las conversiones habilitadas.
// Thread-safe: la UI llama enable/disable; el hilo worker llama onSentence.
class RosPublisher {
public:
    RosPublisher();
    ~RosPublisher();

    void enable(const std::string& talker, const std::string& formatter,
                const RosTarget& target);
    void disable(const std::string& talker, const std::string& formatter);

    void onSentence(const std::string& talker, const std::string& formatter,
                    const std::string& category,
                    const std::vector<std::string>& names,
                    const std::vector<std::string>& values);

private:
    struct Pub;  // definido en el .cpp
    static std::string key(const std::string& t, const std::string& f);
    void ensureNode();  // inicializa rclcpp + nodo en el primer enable (bajo lock)

    std::mutex                                   mu_;
    rclcpp::Node::SharedPtr                      node_;
    std::map<std::string, std::unique_ptr<Pub>>  pubs_;
};

}  // namespace nmea::ros
