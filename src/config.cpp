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

#include <lo2s/log.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/lang/optional.hpp>

#include <boost/optional.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <ctime>   // for CLOCK_* macros
#include <iomanip> // for std::setw

extern "C"
{
#include <unistd.h>
}

#define Q(x) #x
#define QUOTE(x) Q(x)

namespace po = boost::program_options;

namespace lo2s
{

/** A helper type to count how many times a switch has been specified
 *
 **/
struct SwitchCounter
{

    SwitchCounter() : count(0){};
    SwitchCounter(int c) : count(c){};

    unsigned count;
};

void validate(boost::any& v, const std::vector<std::string>&, SwitchCounter*, long)
{
    if (v.empty())
    {
        v = SwitchCounter{ 1 };
    }
    else
    {
        boost::any_cast<SwitchCounter&>(v).count++;
    }
}

static void list_event_names(std::ostream& os, const std::string& category_name)
{
    struct event_category
    {
        using listing_function_t = std::vector<std::string> (*)();

        const char* name;
        const char* description;
        listing_function_t event_list;
    };

    static constexpr event_category CATEGORIES[] = {
        { "predefined", "predefined events", &perf::EventProvider::get_predefined_event_names },
        { "pmu", "Kernel PMU events", &perf::EventProvider::get_pmu_event_names },
        { "tracepoint", "Kernel tracepoint events",
          &perf::tracepoint::EventFormat::get_tracepoint_event_names },
    };

    bool list_all = category_name == "all";
    bool listed_any = false;

    for (const auto category : CATEGORIES)
    {
        if (list_all || category_name == category.name)
        {
            listed_any = true;
            os << "\nList of " << category.description << ":\n\n";

            auto list = category.event_list();

            if (!list.empty())
            {
                std::sort(list.begin(), list.end());
                for (const auto& event : list)
                {
                    os << "  " << event << '\n';
                }
            }
            else
            {
                os << "  (none available)\n";
            }
            os << '\n';
        }
    }

    if (!listed_any)
    {
        std::cerr << "Cannot list events from unknown category \"" << category_name
                  << "\"!\n"
                     "\n"
                     "Known event categories are:\n\n";
        for (const auto category : CATEGORIES)
        {
            std::cerr << "  " << std::setw(20) << std::left << category.name << "("
                      << category.description << ")\n";
        }
        std::cerr << '\n';
    }
}

static inline void print_version(std::ostream& os)
{
    // clang-format off
    os << "lo2s " << lo2s::version() << "\n"
       << "Copyright (C) 2018 Technische Universitaet Dresden, Germany\n"
       << "This is free software; see the source for copying conditions.  There is NO\n"
       << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
    // clang-format on
}

static inline void print_usage(std::ostream& os, const char* name,
                               const po::options_description& desc)
{
    static constexpr char argument_detail[] =
        R"(
Arguments to options:
    PATH        A filesystem path.  It can be either relative to the
                current working directory or absolute.

    EVENT       Name of a perf event.  Format is either
                - for a predefined event:
                    <name>
                - for a kernel PMU event one of:
                    <pmu>/<event>/
                    <pmu>:<event>
                  Kernel PMU events can be found at
                    /sys/bus/event_source/devices/<pmu>/event/<event>.

    TRACEPOINT  Name of a kernel tracepoint event.  Format is either
                    <group>:<name>
                or
                    <group>/<name>.
                Tracepoint events can be found at
                    /sys/kernel/debug/tracing/events/<group>/<name>.
                Accessing tracepoints (even for --list-events=tracepoint)
                usually requires read/execute permissions on
                    /sys/kernel/debug
)";

    // clang-format off
    os << "Usage:\n"
          "  " << name << " [options] ./a.out\n"
          "  " << name << " [options] -- ./a.out --option-to-a-out\n"
          "  " << name << " [options] --pid $(pidof some-process)\n"
          "\n" << desc << argument_detail;
    // clang-format on
}

static nitro::lang::optional<Config> instance;

const Config& config()
{
    return *instance;
}
void parse_program_options(int argc, const char** argv)
{
    po::options_description general_options("Options");
    po::options_description system_wide_options("System-wide monitoring");
    po::options_description sampling_options("Sampling options");
    po::options_description kernel_tracepoint_options("Kernel tracepoint options");
    po::options_description perf_metric_options("perf metric options");
    po::options_description x86_adapt_options("x86_adapt options");

    lo2s::Config config;
    SwitchCounter verbosity;
    bool all_cpus;
    bool disassemble, no_disassemble;
    bool kernel, no_kernel;
    bool list_clockids;
    std::string list_event_category;
    std::uint64_t read_interval_ms;
    std::uint64_t metric_count, metric_frequency = 10;
    std::vector<std::string> x86_adapt_cpu_knobs;

    std::string requested_clock_name;

    // clang-format off
    general_options.add_options()
        ("help,h",
            "Show this help message.")
        ("version",
            "Print version information.")
        ("output-trace,o",
            po::value(&config.trace_path)
                ->value_name("PATH"),
            "Output trace directory. If not specified, lo2s_trace_$(date +%Y-%m-%dT%H-%M-%S) will be used.")
        ("quiet,q",
            po::bool_switch(&config.quiet),
            "Suppress output.")
        ("verbose,v",
            po::value(&verbosity)
                ->zero_tokens(),
            "Verbose output (specify multiple times to get increasingly more verbose output).")
        ("mmap-pages,m",
            po::value(&config.mmap_pages)
                ->value_name("PAGES")
                ->default_value(16),
            "Number of pages to be used by internal buffers.")
        ("clockid,k",
            po::value(&requested_clock_name)
                ->value_name("CLOCKID")
                ->default_value("monotonic-raw"),
            "Clock used for perf timestamps (see --list-clockids).")
        ("list-clockids",
            po::bool_switch(&list_clockids)
                ->default_value(false),
            "List all available clockids.")
        ("list-events",
            po::value(&list_event_category)
                ->value_name("CATEGORY")
                ->implicit_value("all"),
            "List available metric and sampling events from a given category.");

    system_wide_options.add_options()
        ("all-cpus,a",
            po::bool_switch(&all_cpus),
            "System-wide monitoring of all CPUs.");

    sampling_options.add_options()
        ("command",
            po::value(&config.command)
                ->value_name("CMD")
        )

        ("count,c",
            po::value(&config.sampling_period)
                ->value_name("N")
                ->default_value(11010113),
            "Sampling period (in number of events specified by -e).")
        ("event,e",
            po::value(&config.sampling_event)
                ->value_name("EVENT")
                ->default_value("instructions"),
            "Interrupt source event for sampling.")
        ("call-graph,g",
            po::bool_switch(&config.enable_cct),
            "Enable call-graph recording.")
        ("no-ip,n",
            po::bool_switch(&config.suppress_ip),
            "Do not record instruction pointers [NOT CURRENTLY SUPPORTED]")
        ("pid,p",
            po::value(&config.pid)
                ->value_name("PID")
                ->default_value(-1),
            "Attach to process of given PID.")
        ("readout-interval,i",
            po::value(&read_interval_ms)
                ->value_name("MSEC")
                ->default_value(100),
            "Time interval between metric and sampling buffer readouts in milliseconds.")
        ("disassemble",
            po::bool_switch(&disassemble),
            "Enable augmentation of samples with instructions (default if supported).")
        ("no-disassemble",
            po::bool_switch(&no_disassemble),
            "Disable augmentation of samples with instructions.")
        ("kernel",
            po::bool_switch(&kernel),
            "Include events happening in kernel space (default).")
        ("no-kernel",
            po::bool_switch(&no_kernel),
            "Exclude events happening in kernel space.");

    kernel_tracepoint_options.add_options()
        ("tracepoint,t",
            po::value(&config.tracepoint_events)
                ->value_name("TRACEPOINT"),
            "Enable global recording of a raw tracepoint event (usually requires root).");

    perf_metric_options.add_options()
        ("metric-event,E",
            po::value(&config.perf_events)
                ->value_name("EVENT"),
            "Record metrics for this perf event.")
        ("metric-leader",
            po::value(&config.metric_leader)
                ->value_name("EVENT")
                ->default_value(QUOTE(DEFAULT_METRIC_LEADER)),
            "The leading metric event.")
        ("metric-count",
            po::value(&metric_count)
                ->value_name("N"),
            "Number of metric leader events to elapse before reading metric buffer. Prefer this setting over --metric-freqency")
        ("metric-frequency",
            po::value(&metric_frequency)
                ->value_name("HZ"),
            "Number of metric buffer reads per second. When given, it will be used to set an approximate value for --metric-count");

    x86_adapt_options.add_options()
        ("x86-adapt-cpu-knob,x",
         po::value(&x86_adapt_cpu_knobs),
         "Add x86_adapt knobs as recordings. Append #accumulated_last for semantics.");
    // clang-format on

    po::options_description all_options;
    all_options.add(general_options)
        .add(system_wide_options)
        .add(sampling_options)
        .add(kernel_tracepoint_options)
        .add(perf_metric_options)
        .add(x86_adapt_options);

    po::positional_options_description p;
    p.add("command", -1);

    po::variables_map vm;
    try
    {
        po::parsed_options parsed =
            po::command_line_parser(argc, argv).options(all_options).positional(p).run();
        po::store(parsed, vm);
    }
    catch (const po::error& e)
    {
        std::cerr << e.what() << '\n';
        print_usage(std::cerr, argv[0], all_options);
        std::exit(EXIT_FAILURE);
    }
    po::notify(vm);

    if (vm.count("help"))
    {
        print_usage(std::cout, argv[0], all_options);
        std::exit(EXIT_SUCCESS);
    }

    if (vm.count("version"))
    {
        print_version(std::cout);
        std::exit(EXIT_SUCCESS);
    }

    if (config.quiet && verbosity.count != 0)
    {
        lo2s::Log::warn() << "Cannot be quiet and verbose at the same time. Refusing to be quiet.";
        config.quiet = false;
    }

    if (config.quiet)
    {
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::error);
    }
    else
    {
        using sl = nitro::log::severity_level;
        switch (verbosity.count)
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
        if (list_clockids)
        {
            std::cout << "Available clockids:\n";
            for (const auto& clock : lo2s::time::ClockProvider::get_descriptions())
            {
                std::cout << " * " << clock.name << '\n';
            }
            std::exit(EXIT_SUCCESS);
        }

        if (vm.count("list-events"))
        {
            list_event_names(std::cout, list_event_category);
            std::exit(EXIT_SUCCESS);
        }
    }

    if (all_cpus)
    {
        config.monitor_type = lo2s::MonitorType::CPU_SET;
    }
    else
    {
        config.monitor_type = lo2s::MonitorType::PROCESS;
    }

    if (config.monitor_type == lo2s::MonitorType::PROCESS && config.pid == -1 &&
        config.command.empty())
    {
        print_usage(std::cerr, argv[0], all_options);
        std::exit(EXIT_FAILURE);
    }

    if (!perf::EventProvider::has_event(config.sampling_event))
    {
        lo2s::Log::error() << "requested sampling event \'" << config.sampling_event
                           << "\' is not available!";
        std::exit(EXIT_FAILURE); // hmm...
    }

    // time synchronization
    config.use_clockid = false;
    try
    {
        const auto& clock = lo2s::time::ClockProvider::get_clock_by_name(requested_clock_name);

        lo2s::Log::debug() << "Using clock \'" << clock.name << "\'.";
#if defined(USE_PERF_CLOCKID) && !defined(HW_BREAKPOINT_COMPAT)
        config.use_clockid = true;
        config.clockid = clock.id;
#else
        if (!vm["clockid"].defaulted())
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
        lo2s::Log::error() << "Invalid clock requested: " << e.what();
        std::exit(EXIT_FAILURE);
    }

    config.read_interval = std::chrono::milliseconds(read_interval_ms);

    if (no_disassemble && disassemble)
    {
        lo2s::Log::warn() << "Cannot enable and disable disassemble option at the same time.";
        config.disassemble = false;
    }
    else if (no_disassemble)
    {
        config.disassemble = false;
    }
    else // disassemble is default
    {
#ifdef HAVE_RADARE
        config.disassemble = true;
#else
        if (disassemble)
        {
            lo2s::Log::warn() << "Disassemble requested, but not supported by this installation.";
        }
        config.disassemble = false;
#endif
    }

    if (vm.count("metric-count"))
    {
        if (vm.count("metric-frequency"))
        {
            lo2s::Log::error()
                << "Cannot specify metric read period and frequency at the same time.";
            std::exit(EXIT_FAILURE);
        }
        else
        {
            config.metric_use_frequency = false;
            config.metric_count = metric_count;
        }
    }
    else
    {
        config.metric_use_frequency = true;
        config.metric_frequency = metric_frequency;
    }

    if (!perf::EventProvider::has_event(config.metric_leader))
    {
        lo2s::Log::error() << "event '" << config.metric_leader
                           << "' is not available as a metric leader!";
        std::exit(EXIT_FAILURE);
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

    if (!x86_adapt_cpu_knobs.empty())
    {
#ifdef HAVE_X86_ADAPT
        config.x86_adapt_cpu_knobs = std::move(x86_adapt_cpu_knobs);
#else
        std::cerr
            << "lo2s was built without support for x86_adapt; cannot set x86_adapt CPU knobs.";
        std::exit(EXIT_FAILURE);
#endif
    }

    for (int arg = 0; arg < argc - 1; arg++)
    {
        config.command_line.append(argv[arg]);
        config.command_line.append(" ");
    }
    config.command_line.append(argv[argc - 1]);

    instance = std::move(config);
}
} // namespace lo2s
