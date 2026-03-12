#include "DataLogger.h"

#include <cmath>
#include <iostream>

DataLogger::DataLogger(std::unique_ptr<ISensor> sensor,
                       std::unique_ptr<ILogSink> sink,
                                             std::chrono::milliseconds interval,
                                             AlertThresholds thresholds,
                                             std::size_t maxSamples)
    : sensor_(std::move(sensor)),
      sink_(std::move(sink)),
      interval_(interval),
            thresholds_(std::move(thresholds)),
            maxSamples_(maxSamples),
            running_(false),
            acceptedSamples_(0),
            rejectedSamples_(0),
            readFailures_(0),
            writeFailures_(0),
            alertCount_(0),
            hasLastReading_(false) {}

DataLogger::~DataLogger() {
    stop();
}

bool DataLogger::start() {
    if (running_.load()) {
        return true;
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    if (!sink_->open()) {
        return false;
    }

    running_.store(true);
    worker_ = std::thread(&DataLogger::loop, this);
    return true;
}

void DataLogger::stop() {
    running_.store(false);
    if (worker_.joinable()) {
        worker_.join();
    }
    sink_->close();
}

bool DataLogger::isRunning() const {
    return running_.load();
}

LoggerStats DataLogger::stats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_.snapshot();
}

LoggerRuntimeSummary DataLogger::summary() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    LoggerRuntimeSummary runtimeSummary;
    runtimeSummary.stats = stats_.snapshot();
    runtimeSummary.acceptedSamples = acceptedSamples_;
    runtimeSummary.rejectedSamples = rejectedSamples_;
    runtimeSummary.readFailures = readFailures_;
    runtimeSummary.writeFailures = writeFailures_;
    runtimeSummary.alertCount = alertCount_;
    runtimeSummary.hasLastReading = hasLastReading_;
    runtimeSummary.lastReading = lastReading_;
    runtimeSummary.running = running_.load();
    return runtimeSummary;
}

bool DataLogger::exportLogs(const std::string& outputPath) {
    return sink_->exportToCsv(outputPath);
}

void DataLogger::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.reset();
    acceptedSamples_ = 0;
    rejectedSamples_ = 0;
    readFailures_ = 0;
    writeFailures_ = 0;
    alertCount_ = 0;
    hasLastReading_ = false;
    lastReading_ = SensorReading{};
}

bool DataLogger::isReadingValid(const SensorReading& reading) const {
    return std::isfinite(reading.temperatureC) &&
           std::isfinite(reading.lightLux) &&
           reading.lightLux >= 0.0;
}

bool DataLogger::isAlert(const SensorReading& reading) const {
    if (thresholds_.hasMinTemperatureC && reading.temperatureC < thresholds_.minTemperatureC) {
        return true;
    }
    if (thresholds_.hasMaxTemperatureC && reading.temperatureC > thresholds_.maxTemperatureC) {
        return true;
    }
    if (thresholds_.hasMinLightLux && reading.lightLux < thresholds_.minLightLux) {
        return true;
    }
    if (thresholds_.hasMaxLightLux && reading.lightLux > thresholds_.maxLightLux) {
        return true;
    }
    return false;
}

void DataLogger::loop() {
    auto nextTick = std::chrono::steady_clock::now();

    while (running_.load()) {
        SensorReading reading;
        if (!sensor_->read(reading)) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            ++readFailures_;
        } else if (!isReadingValid(reading)) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            ++rejectedSamples_;
        } else {
            const bool alertTriggered = isAlert(reading);
            if (sink_->write(reading)) {
                bool reachedLimit = false;
                std::size_t sampleCount = 0;
                {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.add(reading);
                    ++acceptedSamples_;
                    hasLastReading_ = true;
                    lastReading_ = reading;
                    if (alertTriggered) {
                        ++alertCount_;
                    }
                    reachedLimit = maxSamples_ > 0 && acceptedSamples_ >= maxSamples_;
                    sampleCount = acceptedSamples_;
                }

                if (alertTriggered) {
                    std::cout << "[alert] threshold exceeded at " << sampleCount << " samples\n";
                }

                if (reachedLimit) {
                    running_.store(false);
                    break;
                }
            } else {
                std::lock_guard<std::mutex> lock(statsMutex_);
                ++writeFailures_;
            }
        }

        nextTick += interval_;
        std::this_thread::sleep_until(nextTick);
    }

    sink_->close();
}
