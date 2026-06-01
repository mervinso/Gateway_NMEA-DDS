// src/ui/QoSRecommender.cpp
#include "ui/QoSRecommender.hpp"
#include <cmath>

namespace nmea {

QoSProfile QoSRecommender::recommend(Category category, double rate_hz) noexcept {
    const int deadline_ms = (rate_hz > 0.0)
            ? static_cast<int>(std::round(1500.0 / rate_hz)) : 0;
    const int lifespan_ms = (rate_hz > 0.0)
            ? static_cast<int>(std::round(2000.0 / rate_hz)) : 0;

    switch (category) {
        case Category::Radar:
        case Category::Sounder:
        case Category::Weather:
            return {"state_latched", true, true, deadline_ms, lifespan_ms};

        case Category::GPS:
        case Category::Heading:
        case Category::Velocity:
        case Category::Attitude:
        case Category::Inertial:
            return {"telemetry_fast", false, false, deadline_ms, lifespan_ms};
    }
    return {"telemetry_fast", false, false, 0, 0};
}

}  // namespace nmea
