#pragma once

#include <lo2s/config/as_user_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

namespace lo2s
{

struct ProgramUnderTestConfig
{
    ProgramUnderTestConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);

    // Program-under-test
    AsUserConfig as_user;
    std::vector<std::string> command;
    bool use_python = true;
};

void to_json(nlohmann::json& j, const ProgramUnderTestConfig& config);
} // namespace lo2s
