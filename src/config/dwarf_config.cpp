#include <lo2s/config/dwarf_config.hpp>

#include <lo2s/log.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>

#include <string>

#include <cstdlib>

namespace lo2s
{

DwarfConfig::DwarfConfig(nitro::options::arguments& arguments)
{
    const std::string dwarf_mode = arguments.get("dwarf");
    if (dwarf_mode == "full")
    {
        usage = DwarfUsage::FULL;

        const char* debuginfod_urls = getenv("DEBUGINFOD_URLS");
        if (debuginfod_urls == nullptr)
        {
            Log::warn() << "DEBUGINFOD_URLS not set! Will not be able to lookup debug info via "
                           "debuginfod!";
        }
    }
    else if (dwarf_mode == "local")
    {
        usage = DwarfUsage::LOCAL;

        setenv("DEBUGINFOD_URLS", "", 1);
    }
    else if (dwarf_mode == "none")
    {
        usage = DwarfUsage::NONE;
    }
    else
    {
        Log::error() << "Unknown DWARF mode: " << dwarf_mode;
        std::exit(EXIT_FAILURE);
    }
}

void DwarfConfig::add_parser(nitro::options::parser& parser)
{
    auto& dwarf_options = parser.group("DWARF symbol resolution options");
    dwarf_options
        .option("dwarf",
                "Set DWARF resolve mode. 'none' disables DWARF, 'local' uses only locally cached "
                "debuginfo files, 'full' uses debuginfod to download debug information on demand")
        .default_value("local")
        .metavar("DWARFMODE");
}

void DwarfConfig::check() const
{
#ifndef HAVE_DEBUGINFOD
    if (config.dwarf == DwarfUsage::FULL)
    {
        Log::warn() << "No Debuginfod support available, downgrading to use only local files!";
        config.dwarf = DwarfUsage::LOCAL;

        // If we unset DEBUGINFOD_URLS, it will only use locally cached debug info files
        setenv("DEBUGINFOD_URLS", "", 1);
    }
#endif
}

NLOHMANN_JSON_SERIALIZE_ENUM(DwarfUsage, { { DwarfUsage::NONE, "none" },
                                           { DwarfUsage::LOCAL, "local" },
                                           { DwarfUsage::FULL, "full" } })

void to_json(nlohmann::json& j, const DwarfConfig& config)
{
    j = nlohmann::json({ { "usage", config.usage } });
}
} // namespace lo2s
