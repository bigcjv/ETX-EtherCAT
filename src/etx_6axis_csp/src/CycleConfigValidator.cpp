#include "CycleConfigValidator.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kSettingFile = "/usr/lib/ECPL/Config/Setting.json";
constexpr const char* kEniDirectory = "/usr/lib/ECPL/ENI/";

std::string readFile(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot read cycle configuration: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::string> captureAll(const std::string& text, const std::regex& pattern)
{
    std::vector<std::string> values;
    for (std::sregex_iterator match(text.begin(), text.end(), pattern), end; match != end; ++match) {
        values.push_back((*match)[1].str());
    }
    return values;
}

int requireJsonInteger(const std::string& json, const char* key)
{
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern)) {
        throw std::runtime_error(std::string("missing ") + key + " in Setting.json");
    }
    return std::stoi(match[1].str());
}

void requireDcEnabled(const std::string& json)
{
    const std::regex pattern("\\\"ENABLE_DC\\\"\\s*:\\s*(true|false)", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(json, match, pattern) || match[1].str() != "true") {
        throw std::runtime_error("Setting.json ENABLE_DC must be true");
    }
}

std::string littleEndianCycleData(std::uint64_t nanoseconds)
{
    std::ostringstream data;
    data << std::uppercase << std::hex << std::setfill('0');
    for (int byte = 0; byte < 8; ++byte) {
        data << std::setw(2) << ((nanoseconds >> (byte * 8)) & 0xffU);
    }
    return data.str();
}

void requireValues(const std::vector<std::string>& values,
                   size_t expectedCount,
                   const std::string& expectedValue,
                   const char* label)
{
    if (values.size() != expectedCount
        || !std::all_of(values.begin(), values.end(), [&](const std::string& value) {
               return value == expectedValue;
           })) {
        throw std::runtime_error(std::string("ENI ") + label + " does not match requested axes/cycle");
    }
}

} // namespace

void validateCycleConfiguration(const AppConfig& config)
{
    const std::string setting = readFile(kSettingFile);
    requireDcEnabled(setting);
    if (requireJsonInteger(setting, "CycleTime") != config.cycleTimeUs) {
        throw std::runtime_error("Setting.json CycleTime does not match --cycle-us");
    }

    const std::string eniPath = std::string(kEniDirectory) + config.eniFileName;
    const std::string eni = readFile(eniPath);
    const std::string expectedNs = std::to_string(static_cast<std::uint64_t>(config.cycleTimeUs) * 1000U);
    const size_t axisCount = static_cast<size_t>(config.axisCount);

    requireValues(captureAll(eni, std::regex("<CycleTime0>\\s*([0-9]+)\\s*</CycleTime0>")),
                  axisCount, expectedNs, "CycleTime0");
    requireValues(captureAll(eni, std::regex("<CycleTime1>\\s*([0-9]+)\\s*</CycleTime1>")),
                  axisCount, expectedNs, "CycleTime1");
    requireValues(captureAll(eni, std::regex(
        "<Comment>\\s*set DC cycle time\\s*</Comment>[\\s\\S]*?<Data>\\s*([0-9A-Fa-f]+)\\s*</Data>")),
                  axisCount, littleEndianCycleData(std::stoull(expectedNs)), "DC cycle register writes");
    requireValues(captureAll(eni, std::regex(
        "<Comment>\\s*set DC activation\\s*</Comment>[\\s\\S]*?<Data>\\s*([0-9A-Fa-f]+)\\s*</Data>")),
                  axisCount, "0003", "DC activation writes");

    const auto references = captureAll(
        eni, std::regex("<ReferenceClock>\\s*(true|false)\\s*</ReferenceClock>", std::regex::icase));
    if (references.size() != axisCount || std::count(references.begin(), references.end(), "true") != 1) {
        throw std::runtime_error("ENI must contain exactly one DC reference clock for all requested axes");
    }
}
