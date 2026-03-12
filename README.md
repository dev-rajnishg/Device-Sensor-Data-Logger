# Device Sensor Data Logger (C++)

A C++17 sensor logging application for simulated or serial temperature/light sources. It supports CSV or SQLite persistence, threaded sampling, runtime summaries, threshold alerts, and CSV export.

## Features

- Sensor input modes:
  - Simulated sensor mode (Linux/Windows, no hardware needed)
  - Serial sensor mode (Linux UART/USB serial device)
- Logging outputs:
  - CSV file
  - SQLite database (when SQLite3 is available)
- Runtime controls:
  - Start logging
  - Stop logging
  - Show live stats (min/max/avg)
  - Show runtime status (accepted/rejected samples, failures, last reading)
  - Reset session statistics without restarting the app
  - Export logs to CSV
- Safety and observability:
  - Input validation rejects invalid readings such as `NaN` and negative lux values
  - Configurable alert thresholds for temperature and light
  - Optional `--max-samples` auto-stop for bounded runs and tests
- Demonstrates:
  - OOP interfaces (`ISensor`, `ILogSink`)
  - File I/O and optional SQLite storage
  - Threaded periodic logging loop
  - Basic serial communication parsing

## Architecture

- `ISensor` abstracts data sources. `SimulatedSensor` generates drifting readings. `SerialSensor` reads `temp,light` lines from a serial device on non-Windows platforms.
- `ILogSink` abstracts persistence. `CsvLogSink` appends CSV rows. `SqliteLogSink` stores rows in a `sensor_log` table and can export them back to CSV.
- `DataLogger` owns one sensor and one sink, runs the sampling loop on a worker thread, tracks aggregated stats, validates samples, counts failures, and raises threshold alerts.
- `StatsAggregator` maintains min/max/average for temperature and light.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

The test target covers stats reset logic plus the logger lifecycle, invalid sample rejection, alert tracking, and bounded auto-stop behavior.

## Run

### Interactive mode (simulated sensor + CSV)

```bash
./build/device_sensor_logger --mode sim --output csv --file sensor_log.csv --interval-ms 1000 --temp-alert-max 28 --light-alert-min 100
```

Then use commands:

- `start`
- `stop`
- `stats`
- `status`
- `reset-stats`
- `export exported.csv`
- `quit`

### Timed run (auto start/stop)

```bash
./build/device_sensor_logger --mode sim --output sqlite --file sensor_log.db --duration-sec 20 --max-samples 50 --export out.csv
```

### Serial mode example (Linux)

```bash
./build/device_sensor_logger --mode serial --serial-port /dev/ttyUSB0 --baud 115200 --output csv --file serial_log.csv
```

Expected incoming serial line format:

```text
24.1,315.5
```

## Command Line Options

- `--mode sim|serial`
- `--serial-port <path>`
- `--baud <rate>`
- `--interval-ms <milliseconds>`
- `--output csv|sqlite`
- `--file <output-path>`
- `--duration-sec <seconds>`
- `--max-samples <count>`
- `--temp-alert-min <value>`
- `--temp-alert-max <value>`
- `--light-alert-min <value>`
- `--light-alert-max <value>`
- `--export <csv-path>`

## Example Output

```text
Sensor Data Logger
Commands:
  start              Start logging
  stop               Stop logging
  stats              Show min/max/avg
  status             Show runtime counters and last reading
  reset-stats        Clear collected session statistics
  export <file.csv>  Export log data to CSV
  help               Show commands
  quit               Stop and exit
> start
Logging started.
> status
running=yes accepted=5 rejected=0 read_failures=0 write_failures=0 alerts=1
last=2026-03-12T12:00:05Z temp=28.214C light=92.113 lux
```

## Notes

- If SQLite3 is not found during CMake configure, SQLite output is disabled automatically.
- In serial mode on Linux, ensure your user has permission to access the serial device (for example, group `dialout`).
- On Windows, serial mode is stubbed out in the current implementation and simulated mode should be used unless Windows serial support is added.
