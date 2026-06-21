// src/ros/RosPublisher.cpp
#include "ros/RosPublisher.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {
std::string fieldValue(const std::vector<std::string>& names,
                       const std::vector<std::string>& values,
                       const std::string& name) {
    for (size_t i = 0; i < names.size() && i < values.size(); ++i)
        if (names[i] == name) return values[i];
    return {};
}
double fieldNum(const std::vector<std::string>& names,
                const std::vector<std::string>& values, const char* name) {
    const std::string s = fieldValue(names, values, name);
    return s.empty() ? 0.0 : std::strtod(s.c_str(), nullptr);  // strtod no lanza
}
}  // namespace

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
        const std::vector<std::string>& names,
        const std::vector<std::string>& values,
        const std::string& frame_id,
        const builtin_interfaces::msg::Time& stamp) {
    sensor_msgs::msg::Imu msg;
    msg.header.frame_id = frame_id;
    msg.header.stamp = stamp;

    const double d2r = M_PI / 180.0;
    const double yaw   = fieldNum(names, values, "yaw")   * d2r;
    const double pitch = fieldNum(names, values, "pitch") * d2r;
    const double roll  = fieldNum(names, values, "roll")  * d2r;

    const double cy = std::cos(yaw   * 0.5), sy = std::sin(yaw   * 0.5);
    const double cp = std::cos(pitch * 0.5), sp = std::sin(pitch * 0.5);
    const double cr = std::cos(roll  * 0.5), sr = std::sin(roll  * 0.5);
    msg.orientation.w = cr * cp * cy + sr * sp * sy;
    msg.orientation.x = sr * cp * cy - cr * sp * sy;
    msg.orientation.y = cr * sp * cy + sr * cp * sy;
    msg.orientation.z = cr * cp * sy - sr * sp * cy;
    msg.orientation_covariance = {0.01, 0, 0,  0, 0.01, 0,  0, 0, 0.01};

    msg.angular_velocity_covariance[0]    = -1.0;  // no disponible
    msg.linear_acceleration_covariance[0] = -1.0;  // no disponible
    return msg;
}

sensor_msgs::msg::NavSatFix navSatFixFromFields(
        const std::vector<std::string>& names,
        const std::vector<std::string>& values,
        const std::string& frame_id,
        const builtin_interfaces::msg::Time& stamp) {
    sensor_msgs::msg::NavSatFix msg;
    msg.header.frame_id = frame_id;
    msg.header.stamp = stamp;

    auto toDeg = [](const std::string& ddmm, const std::string& hemi) -> double {
        if (ddmm.empty()) return std::numeric_limits<double>::quiet_NaN();
        const double v   = std::strtod(ddmm.c_str(), nullptr);
        const double deg = std::floor(v / 100.0);
        const double min = v - deg * 100.0;
        double dec = deg + min / 60.0;
        if (hemi == "S" || hemi == "W") dec = -dec;
        return dec;
    };
    msg.latitude  = toDeg(fieldValue(names, values, "latitude"),
                          fieldValue(names, values, "ns_indicator"));
    msg.longitude = toDeg(fieldValue(names, values, "longitude"),
                          fieldValue(names, values, "ew_indicator"));

    const std::string alt = fieldValue(names, values, "altitude");  // solo GGA
    msg.altitude = alt.empty() ? std::numeric_limits<double>::quiet_NaN()
                               : std::strtod(alt.c_str(), nullptr);

    msg.status.status  = sensor_msgs::msg::NavSatStatus::STATUS_FIX;
    msg.status.service = sensor_msgs::msg::NavSatStatus::SERVICE_GPS;
    msg.position_covariance_type =
        sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_UNKNOWN;
    return msg;
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
