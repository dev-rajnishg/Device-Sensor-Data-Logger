#include "Stats.h"

#include <algorithm>
#include <limits>

StatsAggregator::StatsAggregator()
    : minTemp_(std::numeric_limits<double>::infinity()),
      maxTemp_(-std::numeric_limits<double>::infinity()),
      sumTemp_(0.0),
      minLight_(std::numeric_limits<double>::infinity()),
      maxLight_(-std::numeric_limits<double>::infinity()),
      sumLight_(0.0),
      count_(0) {}

void StatsAggregator::add(const SensorReading& reading) {
    minTemp_ = std::min(minTemp_, reading.temperatureC);
    maxTemp_ = std::max(maxTemp_, reading.temperatureC);
    sumTemp_ += reading.temperatureC;

    minLight_ = std::min(minLight_, reading.lightLux);
    maxLight_ = std::max(maxLight_, reading.lightLux);
    sumLight_ += reading.lightLux;

    ++count_;
}

void StatsAggregator::reset() {
    minTemp_ = std::numeric_limits<double>::infinity();
    maxTemp_ = -std::numeric_limits<double>::infinity();
    sumTemp_ = 0.0;

    minLight_ = std::numeric_limits<double>::infinity();
    maxLight_ = -std::numeric_limits<double>::infinity();
    sumLight_ = 0.0;

    count_ = 0;
}

LoggerStats StatsAggregator::snapshot() const {
    LoggerStats stats;
    stats.temperature.count = count_;
    stats.light.count = count_;

    if (count_ == 0) {
        return stats;
    }

    stats.temperature.min = minTemp_;
    stats.temperature.max = maxTemp_;
    stats.temperature.avg = sumTemp_ / static_cast<double>(count_);

    stats.light.min = minLight_;
    stats.light.max = maxLight_;
    stats.light.avg = sumLight_ / static_cast<double>(count_);

    return stats;
}
