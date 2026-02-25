#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace lo2s::perf
{
struct TracepointConfig
{
    TracepointConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);
    static void parse_print_options(nitro::options::arguments& arguments);

    std::vector<std::string> events;
    std::chrono::nanoseconds read_interval = std::chrono::nanoseconds(0);
};

void to_json(nlohmann::json& j, const TracepointConfig& config);
} // namespace lo2s::perf
