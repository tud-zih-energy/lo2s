#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

namespace lo2s
{
enum class DwarfUsage
{
    NONE,
    LOCAL,
    FULL
};

struct DwarfConfig
{

    DwarfConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);
    void check() const;

    DwarfUsage usage = DwarfUsage::NONE;
};

void to_json(nlohmann::json& j, const DwarfConfig& config);
} // namespace lo2s
