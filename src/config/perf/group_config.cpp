#include <lo2s/config/perf/group_config.hpp>

#include <lo2s/log.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdlib>

namespace lo2s::perf::counter
{
GroupConfig::GroupConfig(nitro::options::arguments& arguments)
{
    // Use time interval based metric recording as a default
    if (!arguments.provided("metric-leader"))
    {
        use_frequency = true;
        frequency = arguments.as<std::uint64_t>("metric-frequency");
    }
    else
    {
        if (!arguments.provided("metric-count"))
        {
            Log::fatal() << "--metric-count needs to be provided and not == 0 if a custom metric "
                            "leader is used";
            std::exit(EXIT_FAILURE);
        }
        use_frequency = false;
        count = arguments.as<std::uint64_t>("metric-count");
    }
    leader = arguments.get("metric-leader");

    counters = arguments.get_all("metric-event");
    if (arguments.given("standard-metrics"))
    {
        counters.emplace_back("instructions");
        counters.emplace_back("cpu-cycles");
    }
    if (!leader.empty() || !counters.empty())
    {
        enabled = true;
    }
}

void GroupConfig::add_parser(nitro::options::parser& parser)
{
    auto& perf_group_metric_options = parser.group("perf grouped counter recording options");

    perf_group_metric_options
        .option("metric-leader", "The leading metric event. This event is used as an interval "
                                 "giver for the other metric events.")
        .optional()
        .default_value("")
        .metavar("EVENT");
    perf_group_metric_options
        .option("metric-count", "Number of metric leader events to elapse before reading metric "
                                "buffer. Has to be used in conjunction with --metric-leader")
        .metavar("N")
        .optional();

    perf_group_metric_options
        .option("metric-frequency",
                "Number of metric buffer reads per second. Can not be used with --metric-leader")
        .metavar("HZ")
        .default_value("10");
    perf_group_metric_options.multi_option("metric-event", "Record metrics for this perf event.")
        .short_name("E")
        .optional()
        .metavar("EVENT");

    perf_group_metric_options.toggle("standard-metrics", "Record a set of default metrics.");
}

void GroupConfig::check() const
{
    if (!use_frequency && leader.empty())
    {
        Log::fatal() << "--metric-count can only be used in conjunction with a --metric-leader";
        std::exit(EXIT_FAILURE);
    }

    if (use_frequency && !leader.empty())
    {
        Log::fatal() << "--metric-frequency can only be used with the default --metric-leader";
        std::exit(EXIT_FAILURE);
    }
    if (leader.empty() && frequency == 0)
    {
        Log::fatal()
            << "--metric-frequency should not be zero when using the default metric leader";
        std::exit(EXIT_FAILURE);
    }
    if (!leader.empty() && count == 0)
    {
        Log::fatal()
            << "--metric-count needs to be provided and not == 0 if a custom metric leader is used";
        std::exit(EXIT_FAILURE);
    }
}

void to_json(nlohmann::json& j, const GroupConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled },
                         { "use_frequency", config.use_frequency },
                         { "count", config.count },
                         { "frequency", config.frequency },
                         { "leader", config.leader },
                         { "counters", config.counters } });
}
} // namespace lo2s::perf::counter
