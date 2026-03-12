#include "Sensor.h"

#include "TimeUtils.h"

#include <cmath>
#include <sstream>

#if !defined(_WIN32)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

SimulatedSensor::SimulatedSensor()
    : rng_(std::random_device{}()),
      tempNoise_(0.0, 0.35),
      lightNoise_(0.0, 8.0),
      temperatureC_(24.0),
      lightLux_(320.0) {}

bool SimulatedSensor::read(SensorReading& reading) {
    temperatureC_ += tempNoise_(rng_);
    lightLux_ += lightNoise_(rng_);

    if (lightLux_ < 0.0) {
        lightLux_ = 0.0;
    }

    reading.timestamp = nowUtc();
    reading.temperatureC = temperatureC_;
    reading.lightLux = lightLux_;
    return true;
}

std::string SimulatedSensor::name() const {
    return "simulated";
}

SerialSensor::SerialSensor(std::string portName, int baudRate)
    : portName_(std::move(portName)), baudRate_(baudRate), fd_(-1) {}

SerialSensor::~SerialSensor() {
    closePort();
}

bool SerialSensor::read(SensorReading& reading) {
#if defined(_WIN32)
    (void)reading;
    return false;
#else
    if (fd_ < 0 && !openPort()) {
        return false;
    }

    std::string line;
    char ch = '\0';
    while (true) {
        const ssize_t n = ::read(fd_, &ch, 1);
        if (n <= 0) {
            return false;
        }
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            line.push_back(ch);
        }
    }

    // Expected line format: temp,light (example: 24.1,315.5)
    std::stringstream ss(line);
    std::string t;
    std::string l;
    if (!std::getline(ss, t, ',')) {
        return false;
    }
    if (!std::getline(ss, l, ',')) {
        return false;
    }

    reading.timestamp = nowUtc();
    reading.temperatureC = std::stod(t);
    reading.lightLux = std::stod(l);
    return true;
#endif
}

std::string SerialSensor::name() const {
    return "serial:" + portName_;
}

bool SerialSensor::openPort() {
#if defined(_WIN32)
    return false;
#else
    fd_ = ::open(portName_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        return false;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        closePort();
        return false;
    }

    cfmakeraw(&tty);

    speed_t speed = B115200;
    if (baudRate_ == 9600) {
        speed = B9600;
    } else if (baudRate_ == 19200) {
        speed = B19200;
    } else if (baudRate_ == 38400) {
        speed = B38400;
    } else if (baudRate_ == 57600) {
        speed = B57600;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    tty.c_cflag |= (CLOCAL | CREAD);

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        closePort();
        return false;
    }

    return true;
#endif
}

void SerialSensor::closePort() {
#if !defined(_WIN32)
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}
