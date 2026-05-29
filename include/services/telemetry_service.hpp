#pragma once

#include <string>

namespace services {

class TelemetryService {
public:
    [[nodiscard]] std::string snapshot_json() const;
};

} // namespace services

