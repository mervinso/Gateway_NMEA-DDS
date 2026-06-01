#include <gtest/gtest.h>
#include "ui/QoSRecommender.hpp"

using namespace nmea;

TEST(QoSRecommender, GpsAt1HzIsTelemetryFast) {
    auto p = QoSRecommender::recommend(Category::GPS, 1.0);
    EXPECT_EQ(p.name, "telemetry_fast");
    EXPECT_FALSE(p.reliable);
    EXPECT_FALSE(p.transient_local);
    EXPECT_EQ(p.deadline_ms, 1500);
    EXPECT_EQ(p.lifespan_ms, 2000);
}

TEST(QoSRecommender, RadarIsStateLatched) {
    auto p = QoSRecommender::recommend(Category::Radar, 0.5);
    EXPECT_EQ(p.name, "state_latched");
    EXPECT_TRUE(p.reliable);
    EXPECT_TRUE(p.transient_local);
}

TEST(QoSRecommender, InertialIsTelemetryFastHighRate) {
    auto p = QoSRecommender::recommend(Category::Inertial, 100.0);
    EXPECT_EQ(p.name, "telemetry_fast");
    EXPECT_FALSE(p.reliable);
    EXPECT_EQ(p.deadline_ms, 15);
}

TEST(QoSRecommender, UnknownRateUsesDefaults) {
    auto p = QoSRecommender::recommend(Category::GPS, 0.0);
    EXPECT_EQ(p.name, "telemetry_fast");
    EXPECT_EQ(p.deadline_ms, 0);
}
