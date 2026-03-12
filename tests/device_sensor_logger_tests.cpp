#include "DataLogger.h"
#include "TimeUtils.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace {

class SequenceSensor final : public ISensor {
public:
    explicit SequenceSensor(std::vector<SensorReading> readings)
        : readings_(std::move(readings)), index_(0) {}

    bool read(SensorReading& reading) override {
        if (readings_.empty()) {
            return false;
        }

        if (index_ < readings_.size()) {
            reading = readings_[index_++];
        } else {
            reading = readings_.back();
        }

        if (reading.timestamp.time_since_epoch().count() == 0) {
            reading.timestamp = nowUtc();
        }
        return true;
    }

    std::string name() const override {
        return "sequence";
    }

private:
    std::vector<SensorReading> readings_;
    std::size_t index_;
};

class MemorySink final : public ILogSink {
public:
    bool open() override {
        ++openCount;
        opened = true;
        return true;
    }

    bool write(const SensorReading& reading) override {
        if (!opened) {
            return false;
        }
        writes.push_back(reading);
        return true;
    }

    void close() override {
        ++closeCount;
        opened = false;
    }

    std::string name() const override {
        return "memory";
    }

    bool opened = false;
    int openCount = 0;
    int closeCount = 0;
    std::vector<SensorReading> writes;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testStatsAggregatorReset() {
    StatsAggregator stats;
    stats.add(SensorReading{nowUtc(), 20.0, 100.0});
    stats.add(SensorReading{nowUtc(), 30.0, 200.0});

    const LoggerStats beforeReset = stats.snapshot();
    require(beforeReset.temperature.count == 2, "expected two samples before reset");
    require(beforeReset.temperature.min == 20.0, "expected min temp before reset");
    require(beforeReset.light.max == 200.0, "expected max light before reset");

    stats.reset();
    const LoggerStats afterReset = stats.snapshot();
    require(afterReset.temperature.count == 0, "expected zero samples after reset");
    require(afterReset.temperature.avg == 0.0, "expected zero average after reset");
}

void testLoggerSummaryAndAutoStop() {
    std::vector<SensorReading> readings;
    readings.push_back(SensorReading{nowUtc(), std::nan(""), 100.0});
    readings.push_back(SensorReading{nowUtc(), 21.5, 120.0});
    readings.push_back(SensorReading{nowUtc(), 31.0, 810.0});

    auto sensor = std::make_unique<SequenceSensor>(readings);
    auto sink = std::make_unique<MemorySink>();

    AlertThresholds thresholds;
    thresholds.maxTemperatureC = 30.0;
    thresholds.maxLightLux = 800.0;

    DataLogger logger(std::move(sensor), std::move(sink), std::chrono::milliseconds(1), thresholds, 2);
    require(logger.start(), "logger failed to start");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (logger.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    require(!logger.isRunning(), "logger should auto-stop after max samples");

    logger.stop();
    const LoggerRuntimeSummary summary = logger.summary();
    require(summary.acceptedSamples == 2, "expected two accepted samples");
    require(summary.rejectedSamples == 1, "expected one rejected sample");
    require(summary.alertCount == 1, "expected one threshold alert");
    require(summary.writeFailures == 0, "expected zero write failures");
    require(summary.readFailures == 0, "expected zero read failures");
    require(summary.hasLastReading, "expected a last reading");
    require(summary.stats.temperature.count == 2, "expected two samples in aggregated stats");

    logger.resetStats();
    const LoggerRuntimeSummary resetSummary = logger.summary();
    require(resetSummary.acceptedSamples == 0, "expected accepted samples reset");
    require(resetSummary.alertCount == 0, "expected alert count reset");
    require(!resetSummary.hasLastReading, "expected last reading reset");
}

} // namespace

int main() {
    testStatsAggregatorReset();
    testLoggerSummaryAndAutoStop();
    std::cout << "All tests passed.\n";
    return 0;
}