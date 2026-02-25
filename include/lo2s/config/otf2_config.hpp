#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>

namespace lo2s
{
struct Otf2Config
{
    Otf2Config(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);

    std::string trace_path;
};

void to_json(nlohmann::json& j, const Otf2Config& config);
} // namespace lo2s
