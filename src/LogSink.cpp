#include "LogSink.h"

#include "TimeUtils.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#if defined(HAS_SQLITE)
#include <sqlite3.h>
#endif

CsvLogSink::CsvLogSink(std::string filePath) : filePath_(std::move(filePath)) {}

bool CsvLogSink::open() {
    bool writeHeader = true;
    {
        std::ifstream existing(filePath_, std::ios::in | std::ios::binary);
        if (existing.is_open()) {
            existing.peek();
            writeHeader = existing.eof();
        }
    }

    file_.open(filePath_, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
        return false;
    }
    if (writeHeader) {
        file_ << "timestamp,temperature_c,light_lux\n";
        file_.flush();
    }
    return true;
}

bool CsvLogSink::write(const SensorReading& reading) {
    if (!file_.is_open()) {
        return false;
    }
    file_ << toIso8601(reading.timestamp) << ','
          << std::fixed << std::setprecision(3) << reading.temperatureC << ','
          << std::fixed << std::setprecision(3) << reading.lightLux << '\n';
    return true;
}

void CsvLogSink::close() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

std::string CsvLogSink::name() const {
    return "csv:" + filePath_;
}

bool CsvLogSink::exportToCsv(const std::string& outputPath) {
    if (outputPath == filePath_) {
        return true;
    }

    std::ifstream input(filePath_, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::ofstream output(outputPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << input.rdbuf();
    return output.good();
}

SqliteLogSink::SqliteLogSink(std::string dbPath) : dbPath_(std::move(dbPath)), db_(nullptr) {}

SqliteLogSink::~SqliteLogSink() {
    close();
}

bool SqliteLogSink::open() {
#if !defined(HAS_SQLITE)
    return false;
#else
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
        close();
        return false;
    }

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS sensor_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp TEXT NOT NULL,"
        "temperature_c REAL NOT NULL,"
        "light_lux REAL NOT NULL"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, ddl, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
        }
        close();
        return false;
    }
    return true;
#endif
}

bool SqliteLogSink::write(const SensorReading& reading) {
#if !defined(HAS_SQLITE)
    (void)reading;
    return false;
#else
    if (db_ == nullptr) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO sensor_log(timestamp, temperature_c, light_lux) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    const std::string timestamp = toIso8601(reading.timestamp);
    sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, reading.temperatureC);
    sqlite3_bind_double(stmt, 3, reading.lightLux);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
#endif
}

void SqliteLogSink::close() {
#if defined(HAS_SQLITE)
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
#endif
}

std::string SqliteLogSink::name() const {
    return "sqlite:" + dbPath_;
}

bool SqliteLogSink::exportToCsv(const std::string& outputPath) {
#if !defined(HAS_SQLITE)
    (void)outputPath;
    return false;
#else
    std::ofstream out(outputPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "timestamp,temperature_c,light_lux\n";

    sqlite3* exportDb = db_;
    bool closeAfterExport = false;
    if (exportDb == nullptr) {
        if (sqlite3_open(dbPath_.c_str(), &exportDb) != SQLITE_OK) {
            if (exportDb != nullptr) {
                sqlite3_close(exportDb);
            }
            return false;
        }
        closeAfterExport = true;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT timestamp, temperature_c, light_lux FROM sensor_log ORDER BY id ASC;";
    if (sqlite3_prepare_v2(exportDb, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (closeAfterExport) {
            sqlite3_close(exportDb);
        }
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* ts = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const double temp = sqlite3_column_double(stmt, 1);
        const double light = sqlite3_column_double(stmt, 2);
        out << (ts ? ts : "") << ','
            << std::fixed << std::setprecision(3) << temp << ','
            << std::fixed << std::setprecision(3) << light << '\n';
    }

    sqlite3_finalize(stmt);
    if (closeAfterExport) {
        sqlite3_close(exportDb);
    }
    return true;
#endif
}
