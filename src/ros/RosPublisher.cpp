// src/ros/RosPublisher.cpp
#include "ros/RosPublisher.hpp"

namespace nmea::ros {

struct RosPublisher::Pub {
    RosTarget target;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr        imu;
    rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr  fix;
};

std::string RosPublisher::key(const std::string& t, const std::string& f) {
    return t + '\x1f' + f;  // 0x1F separador de unidad
}

sensor_msgs::msg::Imu imuFromFields(
        const std::vector<std::string>&, const std::vector<std::string>&,
        const std::string&, const builtin_interfaces::msg::Time&) {
    return {};
}

sensor_msgs::msg::NavSatFix navSatFixFromFields(
        const std::vector<std::string>&, const std::vector<std::string>&,
        const std::string&, const builtin_interfaces::msg::Time&) {
    return {};
}

RosPublisher::RosPublisher() = default;
RosPublisher::~RosPublisher() = default;

void RosPublisher::ensureNode() {}
void RosPublisher::enable(const std::string&, const std::string&, const RosTarget&) {}
void RosPublisher::disable(const std::string&, const std::string&) {}
void RosPublisher::onSentence(const std::string&, const std::string&,
                              const std::string&, const std::vector<std::string>&,
                              const std::vector<std::string>&) {}

}  // namespace nmea::ros
