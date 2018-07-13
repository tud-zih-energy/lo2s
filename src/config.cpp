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

#include <lo2s/io.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/util.hpp>
#include <lo2s/version.hpp>

#include <nitro/lang/optional.hpp>

#ifdef HAVE_X86_ADAPT
#include <x86_adapt_cxx/exception.hpp>
#include <x86_adapt_cxx/x86_adapt.hpp>
#endif

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

static inline void list_arguments_sorted(std::ostream& os, const std::string& description,
                                         std::vector<std::string> items)
{
    std::sort(items.begin(), items.end());
    os << io::make_argument_list(description, items.begin(), items.end());
}

static inline void list_x86_adapt_cpu_knobs(std::ostream& os)
{
#ifdef HAVE_X86_ADAPT
    std::vector<std::string> knobs;
    try
    {
        ::x86_adapt::x86_adapt x86_adapt;
        auto cpu_cfg_items = x86_adapt.cpu_configuration_items();

        knobs.reserve(cpu_cfg_items.size());
        for (const auto& item : cpu_cfg_items)
        {
            knobs.emplace_back(item.name());
        }
    }
    catch (const ::x86_adapt::x86_adapt_error& e)
    {
        Log::debug() << "Failed to access x86_adapt CPU knobs! (error: " << e.what() << ")";
    }

    os << io::make_argument_list("x86_adapt CPU knobs", knobs.begin(), knobs.end());
#else
    (void)os;
    std::cerr << "lo2s was built without support for x86_adapt; cannot read CPU knobs.\n";
    std::exit(EXIT_FAILURE);
#endif
}

static inline void list_x86_adapt_node_knobs(std::ostream& os)
{
#ifdef HAVE_X86_ADAPT
    std::vector<std::string> knobs;
    try
    {
        ::x86_adapt::x86_adapt x86_adapt;
        auto node_cfg_items = x86_adapt.node_configuration_items();

        knobs.reserve(node_cfg_items.size());
        for (const auto& item : node_cfg_items)
        {
            knobs.emplace_back(item.name());
        }
    }
    catch (const ::x86_adapt::x86_adapt_error& e)
    {
        Log::debug() << "Failed to access x86_adapt node knobs! (error: " << e.what() << ")";
    }

    os << io::make_argument_list("x86_adapt node knobs", knobs.begin(), knobs.end());
#else
    (void)os;
    std::cerr << "lo2s was built without support for x86_adapt; cannot read node knobs.\n";
    std::exit(EXIT_FAILURE);
#endif
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
    PATH        A filesystem path.  If the pathname given in PATH is
                relative, then  it is  interpreted  relative  to the
                current working  directory of  lo2s, otherwise it is
                interpreted as an absolute path.

                NOTE: PATH arguments given to --output-trace support
                simple variable substitution, where any occurence of
                {<var>} will be replaced before being interpreted.

                For substitution, <var> is one of the following:
                - DATE:       current date (format %Y-%m-%dT%H-%M-%S)
                - HOSTNAME:   current system host name
                - ENV=<VAR>:  contents of environment variable <VAR>

                Example:
                    $ FOO=bar lo2s -o lo2s_{ENV=FOO}_{HOSTNAME} ...
                    > Using trace directory: lo2s_bar_HAL-9000

    EVENT       Name of a perf event.  Format is either
                - for a predefined event:
                    <name>
                - for a kernel PMU event one of:
                    <pmu>/<event>/
                    <pmu>:<event>
                  Kernel PMU events can be found at
                    /sys/bus/event_source/devices/<pmu>/event/<event>.
                - for a raw event:
                    r<NNNN> where NNNN is the hexadecimal identifier
                    of the event

                To list all available event names, use --list-events.

    TRACEPOINT  Name of a kernel tracepoint event.  Format is either
                    <group>:<name>
                or
                    <group>/<name>.
                Tracepoint events can be found at
                    /sys/kernel/debug/tracing/events/<group>/<name>.

                Use --list-tracepoints to get the names of available
                tracepoints events.

                NOTE:  Accessing tracepoint  events usually requires
                read+execute permissions on /sys/kernel/debug.

    CLOCKID     Name of a system clock.  To show a list of available
                clock names, use --list-clockids.

                NOTE: Clock names are determined at compile time and
                might not be available when running on a system that
                is not the build system.

    KNOB        Name of a  x86_adapt  configuration item.  To show a
                list of available knobs, use --list-knobs.
)";

    // clang-format off
    os << "Usage:\n"
          "  " << name << " [options] ./a.out\n"
          "  " << name << " [options] -- ./a.out --option-to-a-out\n"
          "  " << name << " [options] --pid $(pidof some-process)\n"
          "  " << name << " [options] --all-cpus [./a.out]\n"
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
    po::options_description x86_energy_options("x86_energy options");
    po::options_description hidden_options;

    lo2s::Config config;
    SwitchCounter verbosity;
    bool all_cpus;
    bool disassemble, no_disassemble;
    bool kernel, no_kernel;
    bool list_clockids, list_events, list_tracepoints, list_knobs;
    std::uint64_t read_interval_ms;
    std::uint64_t metric_count, metric_frequency = 10;
    std::vector<std::string> x86_adapt_knobs;

    std::string requested_clock_name;

    config.pid = -1; // Default value is set here and not with
                     // po::typed_value::default_value to hide
                     // it from usage message.

    // clang-format off
    general_options.add_options()
        ("help,h",
            "Show this help message.")
        ("version",
            "Print version information.")
        ("output-trace,o",
            po::value(&config.trace_path)
                ->value_name("PATH"),
            "Output trace directory. Defaults to lo2s_trace_{DATE} if not specified.")
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
            "Reference clock used as timestamp source.")
        ("list-clockids",
            po::bool_switch(&list_clockids)
                ->default_value(false),
            "List all available clockids.")
        ("list-events",
            po::bool_switch(&list_events)
                ->default_value(false),
            "List available metric and sampling events.")
        ("list-tracepoints",
            po::bool_switch(&list_tracepoints)
                ->default_value(false),
            "List available kernel tracepoint events.")
        ("list-knobs",
            po::bool_switch(&list_knobs)
                ->default_value(false),
            "List available x86_adapt CPU knobs.");

    system_wide_options.add_options()
        ("all-cpus,a",
            po::bool_switch(&all_cpus),
            "System-wide monitoring of all CPUs. If the name of an executable or a pid is given, monitor as long as it is running");

    sampling_options.add_options()
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
            "Record call stack of instruction samples.")
        ("no-ip,n",
            po::bool_switch(&config.suppress_ip),
            "Do not record instruction pointers [NOT CURRENTLY SUPPORTED]")
        ("pid,p",
            po::value(&config.pid)
                ->value_name("PID"),
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
        ("standard-metrics",
            po::bool_switch(&config.standard_metrics),
            "Enable a set of default metrics.")
        ("metric-leader",
            po::value(&config.metric_leader)
                ->value_name("EVENT"),
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
        ("x86-adapt-knob,x",
            po::value(&x86_adapt_knobs)
                ->value_name("KNOB"),
            "Add x86_adapt knobs as recordings. Append #accumulated_last for semantics.");

    x86_energy_options.add_options()
        ("x86-energy,X",
            po::bool_switch(&config.use_x86_energy),
            "Add x86_energy recordings.");

    hidden_options.add_options()
        ("command",
            po::value(&config.command)
                ->value_name("CMD")
        );
    // clang-format on

    po::options_description visible_options;
    visible_options.add(general_options)
        .add(system_wide_options)
        .add(sampling_options)
        .add(kernel_tracepoint_options)
        .add(perf_metric_options)
        .add(x86_adapt_options)
        .add(x86_energy_options);

    po::positional_options_description p;
    p.add("command", -1);

    po::variables_map vm;
    try
    {
        po::options_description all_options;
        all_options.add(visible_options).add(hidden_options);

        po::parsed_options parsed =
            po::command_line_parser(argc, argv).options(all_options).positional(p).run();
        po::store(parsed, vm);
    }
    catch (const po::error& e)
    {
        std::cerr << e.what() << '\n';
        print_usage(std::cerr, argv[0], visible_options);
        std::exit(EXIT_FAILURE);
    }
    po::notify(vm);

    if (vm.count("help"))
    {
        print_usage(std::cout, argv[0], visible_options);
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
            auto& clockids = time::ClockProvider::get_descriptions();
            std::cout << io::make_argument_list("available clockids", std::begin(clockids),
                                                std::end(clockids));
            std::exit(EXIT_SUCCESS);
        }

        if (list_events)
        {
            list_arguments_sorted(std::cout, "predefined events",
                                  perf::EventProvider::get_predefined_event_names());
            list_arguments_sorted(std::cout, "Kernel PMU events",
                                  perf::EventProvider::get_pmu_event_names());
            std::exit(EXIT_SUCCESS);
        }

        if (list_tracepoints)
        {
            list_arguments_sorted(std::cout, "Kernel tracepoint events",
                                  perf::tracepoint::EventFormat::get_tracepoint_event_names());
            std::exit(EXIT_SUCCESS);
        }

        if (list_knobs)
        {
            list_x86_adapt_cpu_knobs(std::cout);
            list_x86_adapt_node_knobs(std::cout);
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
        print_usage(std::cerr, argv[0], visible_options);
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

    // Determine a suitable default metric leader event if none was given on the
    // command line.
    if (!vm.count("metric-leader"))
    {
        Log::debug() << "guessing metric leader event as none was given by the user...";
        try
        {
            config.metric_leader = perf::EventProvider::get_default_metric_leader_event().name;
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::error() << "Failed to determine a suitable metric leader event: " << e.what();
            Log::error() << "Try manually specifying one with --metric-leader.";
            std::exit(EXIT_FAILURE);
        }
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
        lo2s::Log::error()
            << "lo2s was built without support for x86_adapt; cannot set x86_adapt CPU knobs.\n";
        std::exit(EXIT_FAILURE);
#endif
    }

#ifndef HAVE_X86_ENERGY
    if (config.use_x86_energy)
    {
        lo2s::Log::error()
            << "lo2s was built without support for x86_energy; cannot use x86_energy readings.\n";
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
