#pragma once

#include <chrono>

struct SensorReading {
    std::chrono::system_clock::time_point timestamp;
    double temperatureC = 0.0;
    double lightLux = 0.0;
};
