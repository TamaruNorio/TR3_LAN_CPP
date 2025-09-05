// =============================================
// include/tr3/utils.hpp
// =============================================
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace tr3 {

inline std::string hex_dump(const std::vector<uint8_t>& buf) {
    std::ostringstream oss; oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < buf.size(); ++i) oss << std::setw(2) << (int)buf[i];
    return oss.str();
}

inline std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> out; out.reserve(hex.size()/2);
    for (size_t i=0; i+1<hex.size(); i+=2) {
        uint8_t b = static_cast<uint8_t>(strtoul(hex.substr(i,2).c_str(), nullptr, 16));
        out.push_back(b);
    }
    return out;
}

inline std::string bytes_to_hex(const uint8_t* p, size_t n) {
    std::ostringstream oss; oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < n; ++i) oss << std::setw(2) << (int)p[i];
    return oss.str();
}

// 例: "09/04 18:13:13.316"
inline std::string ts_now() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t tt = system_clock::to_time_t(now);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &tt);
#else
    lt = *std::localtime(&tt);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d:%02d.%03d",
        lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, (int)ms.count());
    return buf;
}

// "020030..." を "02 00 30 ..." に
inline std::string hex_spaced(const std::vector<uint8_t>& v) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ' ';
        oss << std::setw(2) << (int)v[i];
    }
    return oss.str();
}

} // namespace tr3