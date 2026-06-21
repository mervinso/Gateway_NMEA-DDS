#include <gtest/gtest.h>
#include <cmath>
#include "ros/RosPublisher.hpp"

using namespace nmea::ros;

TEST(RosPublisherClass, EnablePublishDisableNoThrow) {
    RosPublisher pub;
    RosTarget tg{RosTarget::Imu, "/imu/data", "imu_link"};
    pub.enable("", "VNYPR", tg);
    pub.onSentence("", "GLL", "GPS", {}, {});           // par no habilitado: no-op
    pub.onSentence("", "VNYPR", "Inertial",
                   {"yaw","pitch","roll"}, {"90.0","0.0","0.0"});  // publica
    pub.disable("", "VNYPR");
    SUCCEED();
}

TEST(RosMapping, NavSatFixFromNmeaDdmm) {
    builtin_interfaces::msg::Time t;
    auto msg = navSatFixFromFields(
        {"latitude","ns_indicator","longitude","ew_indicator"},
        {"0816.249979","N","07932.749980","W"}, "gps", t);
    EXPECT_NEAR(msg.latitude,    8.270833, 1e-4);
    EXPECT_NEAR(msg.longitude, -79.545833, 1e-4);
    EXPECT_EQ(msg.header.frame_id, "gps");
    EXPECT_EQ(msg.status.status, sensor_msgs::msg::NavSatStatus::STATUS_FIX);
    EXPECT_TRUE(std::isnan(msg.altitude));
}

TEST(RosMapping, ImuFromYaw90) {
    builtin_interfaces::msg::Time t;  // stamp cero: no afecta la orientación
    auto msg = imuFromFields({"yaw","pitch","roll"}, {"90.0","0.0","0.0"},
                             "imu_link", t);
    EXPECT_NEAR(msg.orientation.x, 0.0, 1e-6);
    EXPECT_NEAR(msg.orientation.y, 0.0, 1e-6);
    EXPECT_NEAR(msg.orientation.z, 0.70710678, 1e-6);
    EXPECT_NEAR(msg.orientation.w, 0.70710678, 1e-6);
    EXPECT_EQ(msg.header.frame_id, "imu_link");
    EXPECT_DOUBLE_EQ(msg.angular_velocity_covariance[0],    -1.0);
    EXPECT_DOUBLE_EQ(msg.linear_acceleration_covariance[0], -1.0);
}
