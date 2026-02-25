#include <lo2s/config/perf/block_io_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <chrono>

#include <cstdint>

namespace lo2s::perf
{
BlockIOConfig::BlockIOConfig(nitro::options::arguments& arguments)
{
    enabled = arguments.given("block-io");
    read_interval = std::chrono::milliseconds(arguments.as<uint64_t>("block-io-read-interval"));
}

void BlockIOConfig::add_parser(nitro::options::parser& parser)
{
    auto& block_io_options = parser.group("Block I/O recording options");

    block_io_options
        .option("block-io-read-interval",
                "Time in milliseconds between readouts of block I/O events")
        .default_value("100")
        .metavar("MSEC");
    block_io_options.toggle("block-io",
                            "Enable recording of block I/O events (requires access to tracefs)");
}

void to_json(nlohmann::json& j, const BlockIOConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled } });
}
} // namespace lo2s::perf
