#include <lo2s/config/perf/posix_io_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <chrono>

#include <cstdint>

namespace lo2s::perf
{
PosixIOConfig::PosixIOConfig(nitro::options::arguments& arguments)
{
    enabled = arguments.given("posix-io");

    read_interval = std::chrono::milliseconds(arguments.as<uint64_t>("posix-io-read-interval"));
}

void PosixIOConfig::add_parser(nitro::options::parser& parser)
{
    auto& posix_io_options = parser.group("POSIX I/O recording options");
    posix_io_options.toggle("posix-io",
                            "Enable recording of POSIX I/o events (requires access to tracefs)");
    posix_io_options
        .option("posix-io-read-interval",
                "Time in milliseconds between readouts of POSIX I/O events")
        .default_value("100")
        .metavar("MSEC");
}

void to_json(nlohmann::json& j, const PosixIOConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled } });
}
} // namespace lo2s::perf
