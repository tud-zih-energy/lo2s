#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

#include <cstdint>

namespace lo2s::perf::counter
{
struct GroupConfig
{
    GroupConfig(nitro::options::arguments& arguments);

    static void add_parser(nitro::options::parser& parser);

    void check() const;
    bool enabled = false;
    bool use_frequency = true;

    std::uint64_t count = 0;
    std::uint64_t frequency = 0;

    std::string leader;
    std::vector<std::string> counters;
};

void to_json(nlohmann::json& j, const GroupConfig& config);
} // namespace lo2s::perf::counter
