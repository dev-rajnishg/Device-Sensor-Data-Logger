cmake_minimum_required(VERSION 3.16)
project(DeviceSensorDataLogger VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_SQLITE "Enable SQLite logging support" ON)
include(CTest)

find_package(Threads REQUIRED)
if(ENABLE_SQLITE)
  find_package(SQLite3)
endif()

add_library(device_sensor_core
  src/TimeUtils.cpp
  src/Sensor.cpp
  src/LogSink.cpp
  src/Stats.cpp
  src/DataLogger.cpp
)

target_include_directories(device_sensor_core PUBLIC include)
target_link_libraries(device_sensor_core PUBLIC Threads::Threads)

if(SQLite3_FOUND)
  target_compile_definitions(device_sensor_core PRIVATE HAS_SQLITE=1)
  target_link_libraries(device_sensor_core PUBLIC SQLite::SQLite3)
else()
  message(STATUS "SQLite3 not found. SQLite output will be unavailable.")
endif()

add_executable(device_sensor_logger
  src/main.cpp
)

target_link_libraries(device_sensor_logger PRIVATE device_sensor_core)

if(BUILD_TESTING)
  add_executable(device_sensor_logger_tests
    tests/device_sensor_logger_tests.cpp
  )

  target_link_libraries(device_sensor_logger_tests PRIVATE device_sensor_core)
  add_test(NAME device_sensor_logger_tests COMMAND device_sensor_logger_tests)
endif()
