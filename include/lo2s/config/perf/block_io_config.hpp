#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>

namespace lo2s::perf
{
struct BlockIOConfig
{
    BlockIOConfig(nitro::options::arguments& arguments);

    static void add_parser(nitro::options::parser& parser);

    std::chrono::nanoseconds read_interval = std::chrono::nanoseconds(0);
    bool enabled = false;
};

void to_json(nlohmann::json& j, const BlockIOConfig& config);
} // namespace lo2s::perf
