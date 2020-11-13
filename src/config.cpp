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

#include <nitro/broken_options.hpp>

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
    for (auto& ev : events)
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
    nitro.broken_options::parser parser("lo2s", "Lightweight Node-Level Performance Monitoring");
    parser.accept_positionals();
    parser.positional_name("COMMAND");

    auto general_options = parser.group("Options");
    auto system_mode_options = parser.group("System-monitoring mode options");
    auto sampling_options = parser.group("Sampling options");
    auto kernel_tracepoint_options = parser.group("Kernel tracepoint options");
    auto perf_metric_options = parser.group("perf metric options");
    auto x86_adapt_options = parser.group("x86_adapt options");
    auto x86_energy_options = parser.group("x86_energy options");

    lo2s::Config config;

    general_options.toggle("help", "Show this help message.").short_name("h");

    general_options.toggle("version",
            "Print version information.");

    general_options.option("output-trace", "Output trace directory. Defaults to lo2s_trace_{DATE} if not specified.").short_name("o");
    general_options.toggle("quiet", "Suppress output.").short_name("q");
    general_options.toggle("verbose", "Verbose output (specify multiple times to get increasingly more verbose output).").short_name("v");

    general_options.option("mmap-pages", "Number of pages to be used by internal buffers.").short_name("m").default_value("16").metavar("PAGES");
        
    general_options.option("readout-interval", "Amount of time between readouts of interval based monitors, i.e. x86_adapt, x86_energy.").short_name("i").default_value(100).metavar("MSEC");
    
    general_options.option("perf-readout-interval", "Maximum amount of time between readouts of perf based monitors, i.e. sampling, metrics, tracepoints. 0 means interval based readouts are disabled ").short_name("I").default_value(0).metavar("MSEC");
    
    general_options.option("clockid", "Reference clock used as timestamp source.").short_name("k").default_value("monotonic-raw").metavar("CLOCKID");
    
    general_options.option("pid", "Attach to process of given PID.").short_name("p").metavar("PID").default_value(-1);  
    general_options.toggle("list-clockids", "List all available clockids.");
        
    general_options.toggle("list-events", "List available metric and sampling events.");
    general_options.toggle("list-tracepoints", "List available kernel tracepoint events.");

    general_options.toggle("list-knobs", "List available x86_adapt CPU knobs.");

    system_mode_options.toggle("all-cpus", "Start in system-monitoring mode for all CPUs. "
            "Monitor as long as COMMAND is running or until PID exits.").short_name("a");

    system_mode_options.toggle("all-cpus-sampling",
            "System-monitoring mode with instruction sampling. "
            "Shorthand for \"-a --instruction-sampling\".").short_name("A");

    sampling_options.toggle("instruction-sampling",
            "Enable instruction sampling.");

    sampling_options.toggle("no-instruction-sampling",
            "Disable instruction sampling.");

    sampling_options.option("event", "Interrupt source event for sampling.").short_name("e").metavar("EVENT").default_value(Topology::instance().hypervised() ? "cpu-clock" : "instructions"),
    
    sampling_options.option("count", "Sampling period (in number of events specified by -e).").short_name("c").default_value(11010113).metavar("N");
    
    sampling_options.toggle("call-graph",
            po::bool_switch(&config.enable_cct),
            "Record call stack of instruction samples.").short_name("g");

    sampling_options.toggle("no-ip",
            "Do not record instruction pointers [NOT CURRENTLY SUPPORTED]");

    sampling_options.toggle("disassemble", "Enable augmentation of samples with instructions (default if supported).");

    sampling_options.toggle("no-disassemble", "Disable augmentation of samples with instructions.");

    sampling_options.toggle("kernel", "Include events happening in kernel space (default).");

    sampling_options.toggle("no-kernel", "Exclude events happening in kernel space.");

    kernel_tracepoint_options.option
        ("tracepoint", "Enable global recording of a raw tracepoint event (usually requires root).").short_name("t").metavar("TRACEPOINT");

    perf_metric_options.multi_option("metric-event","Record metrics for this perf event.").short_name("E").metavar("EVENT");
        
    perf_metric_options.toggle("standard-metrics", "Enable a set of default metrics.");

    perf_metric_options.option("metric-leader", "The leading metric event.").metavar("EVENT");

    perf_metric_options.option("metric-count","Number of metric leader events to elapse before reading metric buffer. Has to be used in conjunction with --metric-leader").metavar("N").default_value(0);
    
    perf_metric_options.option("metric-frequency", "Number of metric buffer reads per second. Can not be used with --metric-leader").metavar("HZ").default_value(10);

    x86_adapt_options.option("x86-adapt-knob", "Add x86_adapt knobs as recordings. Append #accumulated_last for semantics.").short_name("x").metavar("KNOB");

    x86_energy_options.option("x86-energy", "Add x86_energy recordings.").short_name("X");

    nitro::broken_options::options options;
    try
    {
        options = parser.parse(argc, argv);
    }
    catch (const nitro::broken_options::parsing_error& e)
    {
        std::cerr << e.what() << '\n';
        parser.usage();
        std::exit(EXIT_FAILURE);
    }

    config.trace_path = options.get("output-trace");
    config.quiet = options.given("quiet");
    config.mmap_pages = options.as<std::size_t>("mmap-pages");
    config.pid = options.as<pid_t>("pid");
    config.sampling_event = options.get("event");
    config.sampling_period = options.as<std::uint64_t>("count");
    config.enable_cct = options.given("call-graph");
    config.no_ip = options.given("no-ip");
    config.tracepoint_events = options.get("tracepoint");
    config.perf_events = options.get("metric-event");
    config.standard_metrics = options.given("standard-metrics");
    config.metric_leader = options.get("metric-leader");
    config.use_x86_energy = options.given("x86-energy");
    config.command = options.positionals();

    if (options.given("help"))
    {
        parser.usage();
        std::exit(EXIT_SUCCESS);
    }

    if (options.given("version"))
    {
        print_version(std::cout);
        std::exit(EXIT_SUCCESS);
    }

    if (options.given("quiet") && options.given("verbose"))
    {
        lo2s::Log::warn() << "Cannot be quiet and verbose at the same time. Refusing to be quiet.";
        config.quiet = false;
    }

    if (options.given("quiet"))
    {
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::error);
    }
    else
    {
        using sl = nitro::log::severity_level;
        switch (options.given("verbose"))
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

    // list arguments to options and exit
    {
        if (options.given("list-clockids"))
        {
            auto& clockids = time::ClockProvider::get_descriptions();
            std::cout << io::make_argument_list("available clockids", std::begin(clockids),
                                                std::end(clockids));
            std::exit(EXIT_SUCCESS);
        }

        if (options.given("list-events"))
        {
            print_availability(std::cout, "predefined events",
                               perf::EventProvider::get_predefined_events());
            print_availability(std::cout, "Kernel PMU events",
                               perf::EventProvider::get_pmu_events());

            std::cout << "(* Only available in process-monitoring mode" << std::endl;
            std::cout << "(# Only available in system-monitoring mode" << std::endl;

            std::exit(EXIT_SUCCESS);
        }

        if (options.given("list-tracepoints"))
        {
            list_arguments_sorted(std::cout, "Kernel tracepoint events",
                                  perf::tracepoint::EventFormat::get_tracepoint_event_names());
            std::exit(EXIT_SUCCESS);
        }

        if (options.given("list-knobs"))
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

    if (options.given("instruction-sampling") && options.given("no-instruction-sampling"))
    {
        lo2s::Log::warn() << "Can not enable and disable instruction sampling at the same time";
    }
    if (options.given("all-cpus") || options.given("all-cpus-sampling"))
    {
        config.monitor_type = lo2s::MonitorType::CPU_SET;
        config.sampling = false;

        if (options.given("all-cpus-sampling") || options.given("instruction-sampling"))
        {
            config.sampling = true;
        }
    }
    else
    {
        config.monitor_type = lo2s::MonitorType::PROCESS;
        config.sampling = true;

        if (options.given("no-instruction-sampling"))
        {
            config.sampling = false;
        }
    }

    if (config.monitor_type == lo2s::MonitorType::PROCESS && config.pid == -1 &&
        config.command.empty())
    {
        print_usage(std::cerr, argv[0], visible_options);
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
        std::string requested_clock_name = options.get("clockid");
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

    config.read_interval = std::chrono::milliseconds(options.as<std::uint64_t>("readout-interval"));
    config.perf_read_interval = std::chrono::milliseconds(options.as<std::uint64_t("perf-readout-interval"));

    if (options.given("no-disassemble") && options.given("disassemble"))
    {
        lo2s::Log::warn() << "Cannot enable and disable disassemble option at the same time.";
        config.disassemble = false;
    }
    else if (options.given("no-disassemble"))
    {
        config.disassemble = false;
    }
    else // disassemble is default
    {
#ifdef HAVE_RADARE
        config.disassemble = true;
#else
        if (options.given("disassemble"))
        {
            lo2s::Log::warn() << "Disassemble requested, but not supported by this installation.";
        }
        config.disassemble = false;
#endif
    }
    if (options.as<std::uint64_t>("metric-count") == 0 && options.get("metric-leader") == "")
    {
        Log::fatal() << "--metric-count can only be used in conjunction with a --metric-leader";
        std::exit(EXIT_FAILURE);
    }

    if (vm.count("metric-frequency") && vm.count("metric-leader"))
    {
        Log::fatal() << "--metric-frequency can only be used with the default --metric-leader";
        std::exit(EXIT_FAILURE);
    }
    // Use time interval based metric recording as a default
    if (!vm.count("metric-leader"))
    {
        if (metric_frequency == 0)
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
                // Will be handled later in collect_requested_counters
                config.metric_leader.clear();
            }
        }
        config.metric_use_frequency = true;
        config.metric_frequency = metric_frequency;
    }
    else
    {
        if (metric_count == 0)
        {
            Log::fatal() << "--metric-count should not be zero when using a custom metric leader";
            std::exit(EXIT_FAILURE);
        }

        config.metric_use_frequency = false;
        config.metric_count = metric_count;
    }

    config.exclude_kernel = false;
    if (kernel && no_kernel)
    {
        lo2s::Log::warn() << "Cannot enable and disable kernel events at the same time.";
    }
    else if (no_kernel)
    {
        config.exclude_kernel = true;
    }

    if (!x86_adapt_knobs.empty())
    {
#ifdef HAVE_X86_ADAPT
        config.x86_adapt_knobs = std::move(x86_adapt_knobs);
#else
        lo2s::Log::fatal() << "lo2s was built without support for x86_adapt; "
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

    for (int arg = 0; arg < argc - 1; arg++)
    {
        config.command_line.append(argv[arg]);
        config.command_line.append(" ");
    }
    config.command_line.append(argv[argc - 1]);

    instance = std::move(config);
}
} // namespace lo2s
