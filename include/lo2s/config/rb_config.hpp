#pragma once

#include <lo2s/build_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <string>

#include <cstdint>

namespace lo2s
{
struct RingbufConfig
{
    RingbufConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);

    std::string socket_path;
    std::string injectionlib_path;
    uint64_t size = 0;
    std::chrono::nanoseconds read_interval = std::chrono::nanoseconds(100);
};

void to_json(nlohmann::json& j, const RingbufConfig& config);
} // namespace lo2s
