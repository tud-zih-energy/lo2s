#include <lo2s/config/perf/userspace_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <chrono>

#include <cstdint>

namespace lo2s::perf::counter
{
UserspaceConfig::UserspaceConfig(nitro::options::arguments& arguments)
{
    counters = arguments.get_all("userspace-metric-event");
    enabled = !counters.empty();

    read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("userspace-readout-interval"));
}

void UserspaceConfig::add_parser(nitro::options::parser& parser)
{
    auto& userspace_options = parser.group("perf userspace counter recording options");
    userspace_options
        .multi_option("userspace-metric-event",
                      "Record metrics for this perf event. (Slower, but might "
                      "work for events for which --metric-event doesn't work)")
        .optional()
        .metavar("EVENT");
    userspace_options
        .option("userspace-readout-interval",
                "Readout interval for metrics specified by --userspace-metric-event")
        .metavar("MSEC")
        .default_value("100");
}

void to_json(nlohmann::json& j, const UserspaceConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled },
                         { "read_interval", config.read_interval.count() },
                         { "counters", config.counters } });
}
} // namespace lo2s::perf::counter
