#include <gtest/gtest.h>
#include <algorithm>

#include "pipeline/PublishPlan.hpp"

using namespace nmea;

static Pipeline::QosSettings fast() { return {false, false, 0, 0}; }
static Pipeline::QosSettings latched() { return {true, true, 100, 200}; }

TEST(PublishPlan, EmptyResolvesNothing) {
    PublishPlan plan;
    EXPECT_FALSE(plan.resolve("GP", "GGA").has_value());
    EXPECT_TRUE(plan.active_formatters().empty());
}

TEST(PublishPlan, AddThenResolveReturnsTarget) {
    PublishPlan plan;
    plan.add("GP", "GGA", "gps_proa", latched());

    auto t = plan.resolve("GP", "GGA");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->device_id, "gps_proa");
    EXPECT_TRUE(t->qos.reliable);
    EXPECT_TRUE(t->qos.transient_local);
    EXPECT_EQ(t->qos.deadline_ms, 100);

    // Otro talker de la MISMA trama no está habilitado por separado.
    EXPECT_FALSE(plan.resolve("GN", "GGA").has_value());
}

TEST(PublishPlan, RemoveDisables) {
    PublishPlan plan;
    plan.add("GP", "GGA", "gps_proa", fast());
    plan.remove("GP", "GGA");
    EXPECT_FALSE(plan.resolve("GP", "GGA").has_value());
    EXPECT_TRUE(plan.active_formatters().empty());
}

TEST(PublishPlan, ActiveFormattersDedupAcrossTalkers) {
    PublishPlan plan;
    plan.add("GP", "GGA", "gps_proa", fast());   // mismo formatter
    plan.add("GN", "GGA", "gps_popa", fast());   // distinto talker, mismo tópico
    plan.add("HC", "HDG", "compas",   fast());

    auto f = plan.active_formatters();
    std::sort(f.begin(), f.end());
    ASSERT_EQ(f.size(), 2u);            // GGA aparece una sola vez
    EXPECT_EQ(f[0], "GGA");
    EXPECT_EQ(f[1], "HDG");

    plan.remove("GP", "GGA");
    EXPECT_EQ(plan.active_formatters().size(), 2u);
    plan.remove("GN", "GGA");
    f = plan.active_formatters();
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0], "HDG");
}
