#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>

namespace lo2s
{

struct AcceleratorConfig
{
    AcceleratorConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);

    bool nvidia = false;
    bool openmp = false;
    bool hip = false;
    bool nec = false;
    std::chrono::microseconds nec_read_interval;
    std::chrono::milliseconds nec_check_interval;
};

void to_json(nlohmann::json& j, const AcceleratorConfig& config);
} // namespace lo2s
