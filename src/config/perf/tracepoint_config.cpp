#include <lo2s/config/perf/tracepoint_config.hpp>

#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/util.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <vector>

#include <cstdint>
#include <cstdlib>

namespace lo2s::perf
{
TracepointConfig::TracepointConfig(nitro::options::arguments& arguments)
{
    events = arguments.get_all("tracepoint");
    read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("tracepoint-read-interval"));
}

void TracepointConfig::parse_print_options(nitro::options::arguments& arguments)
{
    if (arguments.given("list-tracepoints"))
    {
        const std::vector<std::string> tracepoints =
            perf::EventResolver::instance().get_tracepoint_event_names();

        if (tracepoints.empty())
        {
            std::cout << "No tracepoints found!\n";
            std::cout << "Make sure that the tracefs is mounted and that you have read-execute "
                         "access to it.\n";
            std::cout << "\n";
            std::cout << "For more information, see the section TRACEPOINT in the man-page\n";
            std::exit(EXIT_FAILURE);
        }
        else
        {
            list_arguments_sorted(std::cout, "Kernel tracepoint events", tracepoints);
            std::exit(EXIT_SUCCESS);
        }
    }
}

void TracepointConfig::add_parser(nitro::options::parser& parser)
{
    auto& tracepoint_options = parser.group("perf tracepoint recording options");
    tracepoint_options
        .multi_option("tracepoint",
                      "Enable global recording of a raw tracepoint event (usually requires root).")
        .short_name("t")
        .optional()
        .metavar("TRACEPOINT");
    tracepoint_options.toggle("list-tracepoints", "List available kernel tracepoint events.");

    tracepoint_options
        .option("tracepoint-read-interval",
                "Time in milliseconds between readouts of POSIX I/O events")
        .default_value("100")
        .metavar("MSEC");
}

void to_json(nlohmann::json& j, const TracepointConfig& config)
{
    j = nlohmann::json(
        { { "events", config.events }, { "read_interval", config.read_interval.count() } });
}
} // namespace lo2s::perf
