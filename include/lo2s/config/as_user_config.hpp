#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>

namespace lo2s
{
struct AsUserConfig
{
    AsUserConfig(nitro::options::arguments& arguments);

    static void add_parser(nitro::options::parser& parser);

    bool drop_root = false;
    std::string user;
};

void to_json(nlohmann::json& j, const AsUserConfig& config);
} // namespace lo2s
