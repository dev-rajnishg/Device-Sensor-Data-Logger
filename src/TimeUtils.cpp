#include "TimeUtils.h"

#include <ctime>
#include <iomanip>
#include <sstream>

std::string toIso8601(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);

#if defined(_WIN32) && defined(_MSC_VER)
    std::tm tmUtc{};
    gmtime_s(&tmUtc, &t);
#else
    std::tm tmUtc{};
    std::tm* tmPtr = std::gmtime(&t);
    if (tmPtr != nullptr) {
        tmUtc = *tmPtr;
    }
#endif

    std::ostringstream out;
    out << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::chrono::system_clock::time_point nowUtc() {
    return std::chrono::system_clock::now();
}
