/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <lo2s/config.hpp>

#include <lo2s/build_config.hpp>
#include <lo2s/io.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/lang/optional.hpp>

#ifdef HAVE_X86_ADAPT
#include <lo2s/metric/x86_adapt/knobs.hpp>
#endif

#include <nitro/options/parser.hpp>

#include <algorithm>
#include <cstdlib>
#include <ctime>   // for CLOCK_* macros
#include <iomanip> // for std::setw

extern "C"
{
#include <unistd.h>
}

namespace lo2s
{
static inline void list_arguments_sorted(std::ostream& os, const std::string& description,
                                         std::vector<std::string> items)
{
    std::sort(items.begin(), items.end());
    os << io::make_argument_list(description, items.begin(), items.end());
}

static inline void print_availability(std::ostream& os, const std::string& description,
                                      std::vector<perf::EventDescription> events)
{
    std::vector<std::string> event_names;
    for (const auto& ev : events)
    {
        if (ev.availability == perf::Availability::UNAVAILABLE)
        {
            continue;
        }
        else if (ev.availability == perf::Availability::PROCESS_MODE)
        {
            event_names.push_back(ev.name + " *");
        }
        else if (ev.availability == perf::Availability::SYSTEM_MODE)
        {
            event_names.push_back(ev.name + " #");
        }
        else
        {
            event_names.push_back(ev.name);
        }
    }
    list_arguments_sorted(os, description, event_names);
}

static inline void print_version(std::ostream& os)
{
    // clang-format off
    os << "lo2s " << lo2s::version() << "\n"
       << "Copyright (C) " LO2S_COPYRIGHT_YEAR " Technische Universitaet Dresden, Germany\n"
       << "This is free software; see the source for copying conditions.  There is NO\n"
       << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
    // clang-format on
}

static nitro::lang::optional<Config> instance;

const Config& config()
{
    return *instance;
}
void parse_program_options(int argc, const char** argv)
{
    std::stringstream description;
    description << "Lightweight Node-Level Performance Monitoring" << std::endl << std::endl;
    description << "  " << argv[0] << " [options] [-a | -A] COMMAND\n  " << argv[0]
                << " [options] [-a | -A] -- COMMAND [args to command...]\n  " << argv[0]
                << " [options] [-a | -A] -p PID\n";

    nitro::options::parser parser("lo2s", description.str());
    parser.accept_positionals();
    parser.positional_metavar("COMMAND");

    auto& general_options = parser.group("Options");
    auto& system_mode_options = parser.group("System-monitoring mode options");
    auto& sampling_options = parser.group("Sampling options");
    auto& perf_metric_options = parser.group("perf metric options");
    auto& kernel_tracepoint_options = parser.group("Kernel tracepoint options");
    auto& x86_adapt_options = parser.group("x86_adapt options");
    auto& x86_energy_options = parser.group("x86_energy options");

    lo2s::Config config;

    general_options.toggle("help", "Show this help message.").short_name("h");

    general_options.toggle("version", "Print version information.").short_name("V");

    general_options.toggle("quiet", "Suppress output.").short_name("q");
    general_options
        .toggle("verbose",
                "Verbose output (specify multiple times to get increasingly more verbose output).")
        .short_name("v");

    general_options.option("output-trace", "Output trace directory.")
        .default_value("lo2s_trace_{DATE}")
        .env("LO2S_OUTPUT_TRACE")
        .short_name("o");

    general_options.option("pid", "Attach to the process with the given PID.")
        .short_name("p")
        .metavar("PID")
        .optional();

    general_options.option("mmap-pages", "Number of pages to be used by internal buffers.")
        .short_name("m")
        .default_value("16")
        .metavar("PAGES");

    general_options
        .option("readout-interval", "Time in milliseconds between readouts of interval based "
                                    "monitors, i.e. x86_adapt, x86_energy.")
        .short_name("i")
        .default_value("100")
        .metavar("MSEC");

    general_options
        .option("perf-readout-interval",
                "Time in milliseconds between readouts of perf based monitors, i.e. sampling, "
                "metrics, tracepoints. If not provided, interval based readouts are disabled.")
        .short_name("I")
        .optional()
        .metavar("MSEC");

    general_options.option("clockid", "Reference clock used as timestamp source.")
        .short_name("k")
        .default_value("monotonic-raw")
        .metavar("CLOCKID");

    general_options.toggle("list-clockids", "List all available clockids.");

    general_options.toggle("list-events", "List available metric and sampling events.");
    general_options.toggle("list-tracepoints", "List available kernel tracepoint events.");

    general_options.toggle("list-knobs", "List available x86_adapt CPU knobs.");

    system_mode_options
        .toggle("all-cpus", "Start in system-monitoring mode for all CPUs. "
                            "Monitor as long as COMMAND is running or until PID exits.")
        .short_name("a");

    system_mode_options
        .toggle("all-cpus-sampling", "System-monitoring mode with instruction sampling. "
                                     "Shorthand for \"-a --instruction-sampling\".")
        .short_name("A");

    sampling_options
        .toggle("instruction-sampling", "Enable instruction sampling. In system monitoring: "
                                        "(default: disabled). In process monitoring:")
        .default_value(true)
        .allow_reverse();

    // sampling_options.toggle("no-instruction-sampling", "Disable instruction sampling.");

    sampling_options.option("event", "Interrupt source event for sampling.")
        .short_name("e")
        .metavar("EVENT")
        .default_value(Topology::instance().hypervised() ? "cpu-clock" : "instructions");

    sampling_options.option("count", "Sampling period (in number of events specified by -e).")
        .short_name("c")
        .default_value("11010113")
        .metavar("N");

    sampling_options.toggle("call-graph", "Record call stack of instruction samples.")
        .short_name("g");

    sampling_options.toggle("no-ip",
                            "Do not record instruction pointers [NOT CURRENTLY SUPPORTED]");

    sampling_options
        .toggle("disassemble", "Enable augmentation of samples with instructions.")
#ifdef HAVE_RADARE
        .default_value(true)
#endif
        .allow_reverse();

    sampling_options.toggle("kernel", "Include events happening in kernel space.")
        .allow_reverse()
        .default_value(true);

    kernel_tracepoint_options
        .multi_option("tracepoint",
                      "Enable global recording of a raw tracepoint event (usually requires root).")
        .short_name("t")
        .optional()
        .metavar("TRACEPOINT");

    perf_metric_options.multi_option("metric-event", "Record metrics for this perf event.")
        .short_name("E")
        .optional()
        .metavar("EVENT");

    perf_metric_options
        .multi_option("userspace-metric-event",
                      "Record metrics for this perf event. (Slower, but might "
                      "work for events for which --metric-event doesn't work)")
        .optional()
        .metavar("EVENT");

    perf_metric_options
        .option("userspace-readout-interval",
                "Readout interval for metrics specified by --userspace-metric-event")
        .metavar("MSEC")
        .default_value("100");

    perf_metric_options.toggle("standard-metrics", "Record a set of default metrics.");

    perf_metric_options
        .option("metric-leader", "The leading metric event. This event is used as an interval "
                                 "giver for the other metric events.")
        .optional()
        .metavar("EVENT");

    perf_metric_options
        .option("metric-count", "Number of metric leader events to elapse before reading metric "
                                "buffer. Has to be used in conjunction with --metric-leader")
        .metavar("N")
        .optional();

    perf_metric_options
        .option("metric-frequency",
                "Number of metric buffer reads per second. Can not be used with --metric-leader")
        .metavar("HZ")
        .default_value("10");

    x86_adapt_options
        .multi_option("x86-adapt-knob",
                      "Record the given x86_adapt knob. Append #accumulated_last for semantics.")
        .short_name("x")
        .optional()
        .metavar("KNOB");

    x86_energy_options.toggle("x86-energy", "Add x86_energy recordings.").short_name("X");

    nitro::options::arguments arguments;
    try
    {
        arguments = parser.parse(argc, argv);
    }
    catch (const nitro::options::parsing_error& e)
    {
        std::cerr << e.what() << '\n';
        parser.usage();
        std::exit(EXIT_FAILURE);
    }

    config.trace_path = arguments.get("output-trace");
    config.quiet = arguments.given("quiet");
    config.mmap_pages = arguments.as<std::size_t>("mmap-pages");
    config.process =
        arguments.provided("pid") ? Process(arguments.as<pid_t>("pid")) : Process::invalid();
    config.sampling_event = arguments.get("event");
    config.sampling_period = arguments.as<std::uint64_t>("count");
    config.enable_cct = arguments.given("call-graph");
    config.suppress_ip = arguments.given("no-ip");
    config.tracepoint_events = arguments.get_all("tracepoint");
    config.perf_group_events = arguments.get_all("metric-event");
    config.perf_userspace_events = arguments.get_all("userspace-metric-event");
    config.standard_metrics = arguments.given("standard-metrics");
    config.use_x86_energy = arguments.given("x86-energy");
    config.command = arguments.positionals();

    if (arguments.given("help"))
    {
        parser.usage();
        std::exit(EXIT_SUCCESS);
    }

    if (arguments.given("version"))
    {
        print_version(std::cout);
        std::exit(EXIT_SUCCESS);
    }

    if (arguments.given("quiet") && arguments.given("verbose"))
    {
        lo2s::Log::warn() << "Cannot be quiet and verbose at the same time. Refusing to be quiet.";
        config.quiet = false;
    }
    else
    {
        if (arguments.given("quiet"))
        {
            lo2s::logging::set_min_severity_level(nitro::log::severity_level::error);
        }
        else
        {
            using sl = nitro::log::severity_level;
            switch (arguments.given("verbose"))
            {
            case 0:
                lo2s::logging::set_min_severity_level(sl::warn);
                break;
            case 1:
                lo2s::Log::info() << "Enabling log-level 'info'";
                lo2s::logging::set_min_severity_level(sl::info);
                break;
            case 2:
                lo2s::Log::info() << "Enabling log-level 'debug'";
                lo2s::logging::set_min_severity_level(sl::debug);
                break;
            case 3:
            default:
                lo2s::Log::info() << "Enabling log-level 'trace'";
                lo2s::logging::set_min_severity_level(sl::trace);
                break;
            }
        }
    }

    // list arguments to arguments and exit
    {
        if (arguments.given("list-clockids"))
        {
            auto& clockids = time::ClockProvider::get_descriptions();
            std::cout << io::make_argument_list("available clockids", std::begin(clockids),
                                                std::end(clockids));
            std::exit(EXIT_SUCCESS);
        }

        if (arguments.given("list-events"))
        {
            print_availability(std::cout, "predefined events",
                               perf::EventProvider::get_predefined_events());
            print_availability(std::cout, "Kernel PMU events",
                               perf::EventProvider::get_pmu_events());

            std::cout << "(* Only available in process-monitoring mode" << std::endl;
            std::cout << "(# Only available in system-monitoring mode" << std::endl;

            std::exit(EXIT_SUCCESS);
        }

        if (arguments.given("list-tracepoints"))
        {
            list_arguments_sorted(std::cout, "Kernel tracepoint events",
                                  perf::tracepoint::EventFormat::get_tracepoint_event_names());
            std::exit(EXIT_SUCCESS);
        }

        if (arguments.given("list-knobs"))
        {
#ifdef HAVE_X86_ADAPT

            std::map<std::string, std::string> node_knobs =
                metric::x86_adapt::x86_adapt_node_knobs();
            std::cout << io::make_argument_list("x86_adapt node knobs", node_knobs.begin(),
                                                node_knobs.end());

            std::map<std::string, std::string> cpu_knobs = metric::x86_adapt::x86_adapt_cpu_knobs();
            std::cout << io::make_argument_list("x86_adapt CPU knobs", cpu_knobs.begin(),
                                                cpu_knobs.end());
            std::exit(EXIT_SUCCESS);
#else
            std::cerr << "lo2s was built without support for x86_adapt; cannot read knobs.\n";
            std::exit(EXIT_FAILURE);
#endif
        }
    }

    for (const auto& event : config.perf_userspace_events)
    {
        auto it =
            std::find(config.perf_group_events.begin(), config.perf_group_events.end(), event);

        if (it != config.perf_group_events.end())
        {
            Log::warn()
                << event
                << " given as both userspace and grouped metric event only using it in userspace "
                   "measuring mode";
            config.perf_group_events.erase(it);
        }
    }

    if (arguments.given("all-cpus") || arguments.given("all-cpus-sampling"))
    {
        config.monitor_type = lo2s::MonitorType::CPU_SET;
        config.sampling = false;

        // The check for instruction sampling is a bit more complicated, because the default value
        // is different depending on the monitoring mode. This check here is only relevant for
        // system monitoring, where the default is to not use intruction sampling. However, the
        // configured default value in Nitro::options is to have instruction sampling enabled.
        // Hence, this check seems a bit odd at first glance.
        if (arguments.given("all-cpus-sampling") ||
            (arguments.provided("instruction-sampling") && arguments.given("instruction-sampling")))
        {
            config.sampling = true;
        }
    }
    else
    {
        config.monitor_type = lo2s::MonitorType::PROCESS;
        config.sampling = true;

        if (!arguments.given("instruction-sampling"))
        {
            config.sampling = false;
        }
    }

    if (config.monitor_type == lo2s::MonitorType::PROCESS && config.process == Process::invalid() &&
        config.command.empty())
    {
        lo2s::Log::fatal() << "No process to monitor provided. "
                              "You need to pass either a COMMAND or a PID.";
        parser.usage(std::cerr);
        std::exit(EXIT_FAILURE);
    }

    if (config.sampling)
    {
        perf::perf_check_disabled();
    }

    if (config.sampling && !perf::EventProvider::has_event(config.sampling_event))
    {
        lo2s::Log::fatal() << "requested sampling event \'" << config.sampling_event
                           << "\' is not available!";
        std::exit(EXIT_FAILURE); // hmm...
    }

    // time synchronization
    config.use_clockid = false;
    try
    {
        std::string requested_clock_name = arguments.get("clockid");
        // large PEBS only works when the clockid isn't set, however the internal clock
        // large PEBS uses should be equal to monotonic-raw, so use monotonic-raw outside
        // of pebs and everything should be in sync
        if (requested_clock_name == "pebs")
        {
            requested_clock_name = "monotonic-raw";
            config.use_pebs = true;
        }
        const auto& clock = lo2s::time::ClockProvider::get_clock_by_name(requested_clock_name);

        lo2s::Log::debug() << "Using clock \'" << clock.name << "\'.";
#if defined(USE_PERF_CLOCKID) && !defined(USE_HW_BREAKPOINT_COMPAT)
        config.use_clockid = true;
        config.clockid = clock.id;
#else
        if (requested_clock_name != "monotonic-raw")
        {
            lo2s::Log::warn() << "This installation was built without support for setting a "
                                 "perf reference clock.";
            lo2s::Log::warn() << "Any parameter to -k/--clockid will only affect the "
                                 "local reference clock.";
        }
#endif
        lo2s::time::Clock::set_clock(clock.id);
    }
    catch (const lo2s::time::ClockProvider::InvalidClock& e)
    {
        lo2s::Log::fatal() << "Invalid clock requested: " << e.what();
        std::exit(EXIT_FAILURE);
    }

    config.read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("readout-interval"));

    config.userspace_read_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("userspace-readout-interval"));

    if (arguments.provided("perf-readout-interval"))
    {
        config.perf_read_interval =
            std::chrono::milliseconds(arguments.as<std::uint64_t>("perf-readout-interval"));
    }

    if (!arguments.given("disassemble"))
    {
        config.disassemble = false;
    }
    else // disassemble is default
    {
#ifdef HAVE_RADARE
        config.disassemble = true;
#else
        if (arguments.provided("disassemble"))
        {
            lo2s::Log::warn() << "Disassemble requested, but not supported by this installation.";
        }
        config.disassemble = false;
#endif
    }

    if (arguments.provided("metric-count") && !arguments.provided("metric-leader"))
    {
        Log::fatal() << "--metric-count can only be used in conjunction with a --metric-leader";
        std::exit(EXIT_FAILURE);
    }

    if (arguments.provided("metric-frequency") && arguments.provided("metric-leader"))
    {
        Log::fatal() << "--metric-frequency can only be used with the default --metric-leader";
        std::exit(EXIT_FAILURE);
    }

    // Use time interval based metric recording as a default
    if (!arguments.provided("metric-leader"))
    {
        if (arguments.as<std::uint64_t>("metric-frequency") == 0)
        {
            Log::fatal()
                << "--metric-frequency should not be zero when using the default metric leader";
            std::exit(EXIT_FAILURE);
        }

        Log::debug() << "checking if cpu-clock is available...";
        try
        {
            config.metric_leader = perf::EventProvider::get_event_by_name("cpu-clock").name;
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "cpu-clock isn't available, trying to use a fallback event";
            try
            {
                config.metric_leader = perf::EventProvider::get_default_metric_leader_event().name;
            }
            catch (const perf::EventProvider::InvalidEvent& e)
            {
                // Will be handled later in collect_requested_group_counters
                config.metric_leader.clear();
            }
        }
        config.metric_use_frequency = true;
        config.metric_frequency = arguments.as<std::uint64_t>("metric-frequency");
    }
    else
    {
        config.metric_leader = arguments.get("metric-leader");

        if (!arguments.provided("metric-count") || arguments.as<int>("metric-count") == 0)
        {
            Log::fatal() << "--metric-count should not be less or equal to zero when using a "
                            "custom metric leader";
            std::exit(EXIT_FAILURE);
        }

        config.metric_use_frequency = false;
        config.metric_count = arguments.as<std::uint64_t>("metric-count");
    }

    config.exclude_kernel = !static_cast<bool>(arguments.given("kernel"));

    if (arguments.count("x86-adapt-knob"))
    {
#ifdef HAVE_X86_ADAPT
        config.x86_adapt_knobs = arguments.get_all("x86-adapt-knob");
#else
        Log::fatal() << "lo2s was built without support for x86_adapt; "
                        "cannot request x86_adapt knobs.\n";
        std::exit(EXIT_FAILURE);
#endif
    }

#ifndef HAVE_X86_ENERGY
    if (config.use_x86_energy)
    {
        lo2s::Log::fatal() << "lo2s was built without support for x86_energy; "
                              "cannot request x86_energy readings.\n";
        std::exit(EXIT_FAILURE);
    }
#endif

    config.command_line =
        nitro::lang::join(arguments.positionals().begin(), arguments.positionals().end());

    instance = std::move(config);
}
} // namespace lo2s
