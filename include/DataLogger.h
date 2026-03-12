#pragma once

#include "LogSink.h"
#include "Sensor.h"
#include "Stats.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

struct AlertThresholds {
    bool hasMinTemperatureC = false;
    double minTemperatureC = 0.0;
    bool hasMaxTemperatureC = false;
    double maxTemperatureC = 0.0;
    bool hasMinLightLux = false;
    double minLightLux = 0.0;
    bool hasMaxLightLux = false;
    double maxLightLux = 0.0;
};

class DataLogger {
public:
    DataLogger(std::unique_ptr<ISensor> sensor,
               std::unique_ptr<ILogSink> sink,
               std::chrono::milliseconds interval,
               AlertThresholds thresholds = {},
               std::size_t maxSamples = 0);
    ~DataLogger();

    bool start();
    void stop();
    bool isRunning() const;

    LoggerStats stats() const;
    LoggerRuntimeSummary summary() const;
    bool exportLogs(const std::string& outputPath);
    void resetStats();

private:
    void loop();
    bool isReadingValid(const SensorReading& reading) const;
    bool isAlert(const SensorReading& reading) const;

    std::unique_ptr<ISensor> sensor_;
    std::unique_ptr<ILogSink> sink_;
    std::chrono::milliseconds interval_;
    AlertThresholds thresholds_;
    std::size_t maxSamples_;

    std::atomic<bool> running_;
    std::thread worker_;

    mutable std::mutex statsMutex_;
    StatsAggregator stats_;
    std::size_t acceptedSamples_;
    std::size_t rejectedSamples_;
    std::size_t readFailures_;
    std::size_t writeFailures_;
    std::size_t alertCount_;
    bool hasLastReading_;
    SensorReading lastReading_;
};
