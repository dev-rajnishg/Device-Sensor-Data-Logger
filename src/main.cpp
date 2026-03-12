#include "DataLogger.h"

#include "TimeUtils.h"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

struct AppConfig {
    std::string mode = "sim";
    std::string serialPort = "/dev/ttyUSB0";
    int baud = 115200;
    int intervalMs = 1000;
    std::string output = "csv";
    std::string outputPath = "sensor_log.csv";
    int durationSec = 0;
    std::size_t maxSamples = 0;
    std::string exportPath;
    AlertThresholds thresholds;
};

bool parseInt(const std::string& value, int& out) {
    try {
        out = std::stoi(value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseSize(const std::string& value, std::size_t& out) {
    try {
        out = static_cast<std::size_t>(std::stoull(value));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseDouble(const std::string& value, double& out) {
    try {
        out = std::stod(value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void printHelp() {
    std::cout << "Commands:\n"
              << "  start              Start logging\n"
              << "  stop               Stop logging\n"
              << "  stats              Show min/max/avg\n"
              << "  status             Show runtime counters and last reading\n"
              << "  reset-stats        Clear collected session statistics\n"
              << "  export <file.csv>  Export log data to CSV\n"
              << "  help               Show commands\n"
              << "  quit               Stop and exit\n";
}

void printStats(const LoggerStats& stats) {
    std::cout << "samples=" << stats.temperature.count << '\n';
    std::cout << "temperature C: min=" << stats.temperature.min
              << " max=" << stats.temperature.max
              << " avg=" << stats.temperature.avg << '\n';
    std::cout << "light lux:    min=" << stats.light.min
              << " max=" << stats.light.max
              << " avg=" << stats.light.avg << '\n';
}

void printSummary(const LoggerRuntimeSummary& summary) {
    std::cout << "running=" << (summary.running ? "yes" : "no")
              << " accepted=" << summary.acceptedSamples
              << " rejected=" << summary.rejectedSamples
              << " read_failures=" << summary.readFailures
              << " write_failures=" << summary.writeFailures
              << " alerts=" << summary.alertCount << '\n';

    if (summary.hasLastReading) {
        std::cout << "last=" << toIso8601(summary.lastReading.timestamp)
                  << " temp=" << std::fixed << std::setprecision(3) << summary.lastReading.temperatureC
                  << "C light=" << std::fixed << std::setprecision(3) << summary.lastReading.lightLux
                  << " lux\n";
    }
}

bool parseArgs(int argc, char** argv, AppConfig& cfg) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](std::string& out) -> bool {
            if (i + 1 >= argc) {
                return false;
            }
            out = argv[++i];
            return true;
        };

        if (arg == "--mode") {
            if (!next(cfg.mode)) return false;
        } else if (arg == "--serial-port") {
            if (!next(cfg.serialPort)) return false;
        } else if (arg == "--baud") {
            std::string value;
            if (!next(value)) return false;
            if (!parseInt(value, cfg.baud)) return false;
        } else if (arg == "--interval-ms") {
            std::string value;
            if (!next(value)) return false;
            if (!parseInt(value, cfg.intervalMs)) return false;
        } else if (arg == "--output") {
            if (!next(cfg.output)) return false;
        } else if (arg == "--file") {
            if (!next(cfg.outputPath)) return false;
        } else if (arg == "--duration-sec") {
            std::string value;
            if (!next(value)) return false;
            if (!parseInt(value, cfg.durationSec)) return false;
        } else if (arg == "--max-samples") {
            std::string value;
            if (!next(value)) return false;
            if (!parseSize(value, cfg.maxSamples)) return false;
        } else if (arg == "--export") {
            if (!next(cfg.exportPath)) return false;
        } else if (arg == "--temp-alert-min") {
            std::string value;
            if (!next(value)) return false;
            if (!parseDouble(value, cfg.thresholds.minTemperatureC)) return false;
            cfg.thresholds.hasMinTemperatureC = true;
        } else if (arg == "--temp-alert-max") {
            std::string value;
            if (!next(value)) return false;
            if (!parseDouble(value, cfg.thresholds.maxTemperatureC)) return false;
            cfg.thresholds.hasMaxTemperatureC = true;
        } else if (arg == "--light-alert-min") {
            std::string value;
            if (!next(value)) return false;
            if (!parseDouble(value, cfg.thresholds.minLightLux)) return false;
            cfg.thresholds.hasMinLightLux = true;
        } else if (arg == "--light-alert-max") {
            std::string value;
            if (!next(value)) return false;
            if (!parseDouble(value, cfg.thresholds.maxLightLux)) return false;
            cfg.thresholds.hasMaxLightLux = true;
        } else if (arg == "--help" || arg == "-h") {
            return false;
        } else {
            std::cerr << "Unknown arg: " << arg << '\n';
            return false;
        }
    }

    if (cfg.mode != "sim" && cfg.mode != "serial") {
        return false;
    }
    if (cfg.output != "csv" && cfg.output != "sqlite") {
        return false;
    }
    if (cfg.intervalMs <= 0 || cfg.durationSec < 0) {
        return false;
    }

    return true;
}

void printUsage() {
    std::cout << "Usage: device_sensor_logger [options]\n"
              << "  --mode sim|serial\n"
              << "  --serial-port /dev/ttyUSB0\n"
              << "  --baud 115200\n"
              << "  --interval-ms 1000\n"
              << "  --output csv|sqlite\n"
              << "  --file sensor_log.csv|sensor_log.db\n"
              << "  --duration-sec 20\n"
              << "  --max-samples 100\n"
              << "  --temp-alert-min 10\n"
              << "  --temp-alert-max 30\n"
              << "  --light-alert-min 50\n"
              << "  --light-alert-max 800\n"
              << "  --export exported.csv\n";
}

} // namespace

int main(int argc, char** argv) {
    AppConfig cfg;
    if (!parseArgs(argc, argv, cfg)) {
        printUsage();
        return 1;
    }

    std::unique_ptr<ISensor> sensor;
    if (cfg.mode == "serial") {
        sensor = std::make_unique<SerialSensor>(cfg.serialPort, cfg.baud);
    } else {
        sensor = std::make_unique<SimulatedSensor>();
    }

    std::unique_ptr<ILogSink> sink;
    if (cfg.output == "sqlite") {
        sink = std::make_unique<SqliteLogSink>(cfg.outputPath);
    } else {
        sink = std::make_unique<CsvLogSink>(cfg.outputPath);
    }

    DataLogger logger(std::move(sensor),
                      std::move(sink),
                      std::chrono::milliseconds(cfg.intervalMs),
                      cfg.thresholds,
                      cfg.maxSamples);

    if (cfg.durationSec > 0) {
        if (!logger.start()) {
            std::cerr << "Failed to start logger.\n";
            return 2;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(cfg.durationSec);
        while (logger.isRunning() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        logger.stop();

        printStats(logger.stats());
        printSummary(logger.summary());
        if (!cfg.exportPath.empty()) {
            if (logger.exportLogs(cfg.exportPath)) {
                std::cout << "Exported to " << cfg.exportPath << '\n';
            } else {
                std::cout << "Export failed.\n";
            }
        }
        return 0;
    }

    std::cout << "Sensor Data Logger\n";
    printHelp();

    std::string cmd;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, cmd)) {
            break;
        }

        if (cmd == "start") {
            if (logger.start()) {
                std::cout << "Logging started.\n";
            } else {
                std::cout << "Failed to start logging.\n";
            }
        } else if (cmd == "stop") {
            logger.stop();
            std::cout << "Logging stopped.\n";
        } else if (cmd == "stats") {
            printStats(logger.stats());
        } else if (cmd == "status") {
            printSummary(logger.summary());
        } else if (cmd == "reset-stats") {
            logger.resetStats();
            std::cout << "Session statistics reset.\n";
        } else if (cmd.rfind("export ", 0) == 0) {
            const std::string path = cmd.substr(7);
            if (logger.exportLogs(path)) {
                std::cout << "Exported to " << path << '\n';
            } else {
                std::cout << "Export failed.\n";
            }
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd.empty()) {
            continue;
        } else {
            std::cout << "Unknown command. Type 'help'.\n";
        }
    }

    logger.stop();
    return 0;
}
