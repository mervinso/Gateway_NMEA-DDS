#include <gtest/gtest.h>

#include "registry/Registry.hpp"

using namespace nmea;

TEST(Registry, LooksUpKnownFormatterGGA) {
    const Registry reg = Registry::builtin();
    const SentenceDef* def = reg.lookup("GGA");

    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->formatter, "GGA");
    EXPECT_EQ(def->category, Category::GPS);
}

TEST(Registry, ReturnsNullForUnknownFormatter) {
    const Registry reg = Registry::builtin();
    EXPECT_EQ(reg.lookup("XYZ"), nullptr);
    EXPECT_EQ(reg.lookup(""),    nullptr);
}

TEST(Registry, GgaHasCorrectFieldCountAndNames) {
    const Registry reg = Registry::builtin();
    const SentenceDef* def = reg.lookup("GGA");

    ASSERT_NE(def, nullptr);
    ASSERT_EQ(def->fields.size(), 14u);
    EXPECT_EQ(def->fields[0].name, "utc_time");
    EXPECT_EQ(def->fields[0].type, FieldType::Float64);
    EXPECT_EQ(def->fields[0].unit, "hhmmss.ss");
    EXPECT_EQ(def->fields[8].name, "altitude");
    EXPECT_EQ(def->fields[8].unit, "m");
    EXPECT_EQ(def->fields[12].name, "dgps_age");   // campo que puede ser vacío en wire
}

TEST(Registry, VectorNavProprietaryIsInertialCategory) {
    const Registry reg = Registry::builtin();
    const SentenceDef* def = reg.lookup("VNYMR");

    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->category, Category::Inertial);
    ASSERT_EQ(def->fields.size(), 3u);
    EXPECT_EQ(def->fields[0].name, "magnetic_heading");
    EXPECT_EQ(def->fields[0].unit, "deg");
}

TEST(Registry, AddExtendsRegistryAtRuntime) {
    Registry reg = Registry::builtin();

    ASSERT_EQ(reg.lookup("MYPS"), nullptr);  // no existe antes

    reg.add(SentenceDef{
        "MYPS",
        Category::Inertial,
        {{"custom_field", FieldType::Float64, "m/s"}},
    });

    const SentenceDef* def = reg.lookup("MYPS");
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->category, Category::Inertial);
    EXPECT_EQ(def->fields[0].name, "custom_field");
}

TEST(Registry, BuiltinCoversAllCategories) {
    const Registry reg = Registry::builtin();

    // Verifica al menos un formatter por categoría (spot check).
    EXPECT_NE(reg.lookup("GGA"),   nullptr);  // GPS
    EXPECT_NE(reg.lookup("MWV"),   nullptr);  // Weather
    EXPECT_NE(reg.lookup("HDT"),   nullptr);  // Heading
    EXPECT_NE(reg.lookup("TTM"),   nullptr);  // Radar
    EXPECT_NE(reg.lookup("DBT"),   nullptr);  // Sounder
    EXPECT_NE(reg.lookup("VHW"),   nullptr);  // Velocity
    EXPECT_NE(reg.lookup("VNYMR"), nullptr);  // Inertial (propietario VectorNav)
    EXPECT_NE(reg.lookup("APB"),   nullptr);  // Autopilot
    EXPECT_NE(reg.lookup("RPM"),   nullptr);  // Engine
    EXPECT_NE(reg.lookup("VDM"),   nullptr);  // AIS
}

TEST(Registry, SimulatorExtraSentencesResolveToTheirCategories) {
    const Registry reg = Registry::builtin();

    const SentenceDef* apb = reg.lookup("APB");
    ASSERT_NE(apb, nullptr);
    EXPECT_EQ(apb->category, Category::Autopilot);

    const SentenceDef* rpm = reg.lookup("RPM");
    ASSERT_NE(rpm, nullptr);
    EXPECT_EQ(rpm->category, Category::Engine);
    EXPECT_EQ(rpm->fields[2].name, "speed");
    EXPECT_EQ(rpm->fields[2].unit, "rpm");

    // AIS: VDM y VDO comparten estructura (6 campos), categoría AIS.
    const SentenceDef* vdm = reg.lookup("VDM");
    const SentenceDef* vdo = reg.lookup("VDO");
    ASSERT_NE(vdm, nullptr);
    ASSERT_NE(vdo, nullptr);
    EXPECT_EQ(vdm->category, Category::AIS);
    EXPECT_EQ(vdo->category, Category::AIS);
    ASSERT_EQ(vdm->fields.size(), 6u);
    EXPECT_EQ(vdm->fields[4].name, "payload");
}
