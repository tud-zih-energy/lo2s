#pragma once

#include <lo2s/config/monitor_type.hpp>
#include <lo2s/types/process.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>

namespace lo2s
{

struct GeneralConfig
{
    static void parse_print_options(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);
    static void set_verbosity(nitro::options::arguments& arguments);

    GeneralConfig(nitro::options::arguments& arguments, int argc, const char** argv);

    bool quiet = false;
    MonitorType monitor_type = MonitorType::PROCESS;
    Process process;
    std::string lo2s_command_line;
};

void to_json(nlohmann::json& j, const GeneralConfig& config);
} // namespace lo2s
