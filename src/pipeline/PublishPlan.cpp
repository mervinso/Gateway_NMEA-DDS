#include "pipeline/PublishPlan.hpp"

#include <algorithm>

namespace nmea {

void PublishPlan::add(const std::string& talker, const std::string& formatter,
                      const std::string& device_id,
                      const Pipeline::QosSettings& qos) {
    std::lock_guard<std::mutex> lk(mu_);
    entries_[key(talker, formatter)] = Target{device_id, qos};
}

void PublishPlan::remove(const std::string& talker, const std::string& formatter) {
    std::lock_guard<std::mutex> lk(mu_);
    entries_.erase(key(talker, formatter));
}

std::optional<PublishPlan::Target> PublishPlan::resolve(
        const std::string& talker, const std::string& formatter) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = entries_.find(key(talker, formatter));
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

bool PublishPlan::contains(const std::string& talker,
                           const std::string& formatter) const {
    std::lock_guard<std::mutex> lk(mu_);
    return entries_.count(key(talker, formatter)) > 0;
}

std::vector<std::string> PublishPlan::active_formatters() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out;
    for (const auto& [k, t] : entries_) {
        const auto sep = k.find('\x1f');
        std::string fmt = (sep == std::string::npos) ? k : k.substr(sep + 1);
        if (std::find(out.begin(), out.end(), fmt) == out.end())
            out.push_back(std::move(fmt));
    }
    return out;
}

}  // namespace nmea
