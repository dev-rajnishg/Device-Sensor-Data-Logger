#pragma once

#include "Reading.h"

#include <random>
#include <string>

class ISensor {
public:
    virtual ~ISensor() = default;
    virtual bool read(SensorReading& reading) = 0;
    virtual std::string name() const = 0;
};

class SimulatedSensor final : public ISensor {
public:
    SimulatedSensor();
    bool read(SensorReading& reading) override;
    std::string name() const override;

private:
    std::mt19937 rng_;
    std::normal_distribution<double> tempNoise_;
    std::normal_distribution<double> lightNoise_;
    double temperatureC_;
    double lightLux_;
};

class SerialSensor final : public ISensor {
public:
    SerialSensor(std::string portName, int baudRate);
    ~SerialSensor() override;

    bool read(SensorReading& reading) override;
    std::string name() const override;

private:
    std::string portName_;
    int baudRate_;
    int fd_;

    bool openPort();
    void closePort();
};
