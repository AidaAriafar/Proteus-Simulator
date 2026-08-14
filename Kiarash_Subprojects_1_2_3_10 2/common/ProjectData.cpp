#include "common/ProjectData.h"

#include <atomic>
#include <iomanip>
#include <sstream>

namespace kiarash
{

std::string generateID(const std::string& prefix)
{
    static std::atomic<unsigned long long> counter{1};
    const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return prefix + "-" + std::to_string(ticks) + "-" + std::to_string(counter++);
}

Timestamp now()
{
    return std::chrono::system_clock::now();
}

std::string timestampToString(Timestamp timestamp)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    return std::to_string(seconds);
}

Timestamp timestampFromString(const std::string& text)
{
    long long seconds = 0;
    std::istringstream stream(text);
    stream >> seconds;
    return Timestamp(std::chrono::seconds(seconds));
}

}
