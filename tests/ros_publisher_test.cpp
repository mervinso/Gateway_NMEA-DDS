#include <gtest/gtest.h>
#include <cmath>
#include "ros/RosPublisher.hpp"

using namespace nmea::ros;

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
