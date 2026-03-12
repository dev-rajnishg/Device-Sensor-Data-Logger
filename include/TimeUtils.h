#pragma once

#include <chrono>
#include <string>

std::string toIso8601(std::chrono::system_clock::time_point tp);
std::chrono::system_clock::time_point nowUtc();
