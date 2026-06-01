// src/ui/QoSRecommender.hpp
#pragma once
#include <string>
#include "registry/Registry.hpp"

namespace nmea {

struct QoSProfile {
    std::string name;
    bool        reliable{false};
    bool        transient_local{false};
    int         deadline_ms{0};
    int         lifespan_ms{0};
};

class QoSRecommender {
public:
    static QoSProfile recommend(Category category, double rate_hz) noexcept;
};

}  // namespace nmea
