#pragma once

#include "Reading.h"

#include <cstddef>

struct ChannelStats {
    double min = 0.0;
    double max = 0.0;
    double avg = 0.0;
    std::size_t count = 0;
};

struct LoggerStats {
    ChannelStats temperature;
    ChannelStats light;
};

struct LoggerRuntimeSummary {
    LoggerStats stats;
    std::size_t acceptedSamples = 0;
    std::size_t rejectedSamples = 0;
    std::size_t readFailures = 0;
    std::size_t writeFailures = 0;
    std::size_t alertCount = 0;
    bool hasLastReading = false;
    SensorReading lastReading{};
    bool running = false;
};

class StatsAggregator {
public:
    StatsAggregator();
    void add(const SensorReading& reading);
    void reset();
    LoggerStats snapshot() const;

private:
    double minTemp_;
    double maxTemp_;
    double sumTemp_;

    double minLight_;
    double maxLight_;
    double sumLight_;

    std::size_t count_;
};
