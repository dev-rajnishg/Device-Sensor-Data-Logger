#pragma once

#include "Reading.h"

#include <fstream>
#include <memory>
#include <string>

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual bool open() = 0;
    virtual bool write(const SensorReading& reading) = 0;
    virtual void close() = 0;
    virtual std::string name() const = 0;
    virtual bool exportToCsv(const std::string& outputPath) { (void)outputPath; return false; }
};

class CsvLogSink final : public ILogSink {
public:
    explicit CsvLogSink(std::string filePath);
    bool open() override;
    bool write(const SensorReading& reading) override;
    void close() override;
    std::string name() const override;
    bool exportToCsv(const std::string& outputPath) override;

private:
    std::string filePath_;
    std::ofstream file_;
};

class SqliteLogSink final : public ILogSink {
public:
    explicit SqliteLogSink(std::string dbPath);
    ~SqliteLogSink() override;

    bool open() override;
    bool write(const SensorReading& reading) override;
    void close() override;
    std::string name() const override;
    bool exportToCsv(const std::string& outputPath) override;

private:
    std::string dbPath_;

#if defined(HAS_SQLITE)
    struct sqlite3* db_;
#else
    void* db_;
#endif
};
