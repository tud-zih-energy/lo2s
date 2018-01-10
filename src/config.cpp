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
#include <lo2s/perf/util.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/util.hpp>

#include <nitro/lang/optional.hpp>

#include <boost/program_options.hpp>

#include <cstdlib>
#include <ctime> // for CLOCK_* macros

extern "C" {
#include <unistd.h>
}
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

static inline void print_usage(std::ostream& os, const char* name,
                               const po::options_description& desc)
{
    // clang-format off
    os << "Usage:\n"
          "  " << name << " [options] ./a.out\n"
          "  " << name << " [options] -- ./a.out --option-to-a-out\n"
          "  " << name << " [options] --pid $(pidof some-process)\n"
          "\n" << desc;
    // clang-format on
}

static nitro::lang::optional<Config> instance;

const Config& config()
{
    return *instance;
}
void parse_program_options(int argc, const char** argv)
{
    po::options_description desc("Allowed options");

    lo2s::Config config;
    SwitchCounter verbosity;
    bool quiet;
    bool all_cpus;
    bool disassemble, no_disassemble;
    bool kernel, no_kernel;
    bool list_clockids;
    std::uint64_t read_interval_ms;

    std::string requested_clock_name;

    // clang-format off
    desc.add_options()
        ("help",
             "produce help message")
        ("output-trace,o", po::value(&config.trace_path),
             "output trace directory")
        ("count,c", po::value(&config.sampling_period)->default_value(11010113),
             "sampling period (# of events specified by -e)")
        ("event,e", po::value(&config.sampling_event)->default_value("instructions"),
             "interrupt source event for sampling")
        ("all-cpus,a", po::bool_switch(&all_cpus),
             "System-wide monitoring of all CPUs.")
        ("call-graph,g", po::bool_switch(&config.enable_cct),
             "call-graph recording")
        ("no-ip,n", po::bool_switch(&config.suppress_ip),
             "do not record instruction pointers [NOT CURRENTLY SUPPORTED]")
        ("pid,p", po::value(&config.pid)->default_value(-1),
             "attach to specific pid")
        ("quiet,q", po::bool_switch(&quiet),
             "suppress output")
        ("verbose,v", po::value(&verbosity)->zero_tokens(),
             "verbose output (specify multiple times to get increasingly more verbose output)")
        ("mmap-pages,m", po::value(&config.mmap_pages)->default_value(16),
             "number of pages to be used by each internal buffer")
        ("readout-interval,i", po::value(&read_interval_ms)->default_value(100),
             "time interval between metric and sampling buffer readouts in milliseconds")
        ("tracepoint,t", po::value(&config.tracepoint_events),
             "enable global recording of a raw tracepoint event (usually requires root)")
        ("metric-event,E", po::value(&config.perf_events),
             "the name of a perf event to measure") // TODO: optionally list available events
        ("clockid,k",
             po::value(&requested_clock_name)->default_value("monotonic-raw"),
             "clock used for perf timestamps (see --list_clockids for supported arguments)")
#ifdef HAVE_X86_ADAPT // I am going to burn in hell for this
        ("x86-adapt-cpu-knob,x", po::value(&config.x86_adapt_cpu_knobs),
             "add x86_adapt knobs as recordings. Append #accumulated_last for semantics.")
#endif
        ("disassemble", po::bool_switch(&disassemble),
             "enable augmentation of samples with instructions (default if supported)")
        ("no-disassemble", po::bool_switch(&no_disassemble),
             "disable augmentation of samples with instructions")
        ("kernel", po::bool_switch(&kernel),
             "include events happening in kernel space (default)")
        ("no-kernel", po::bool_switch(&no_kernel),
             "exclude events happening in kernel space")
        ("list-clockids", po::bool_switch(&list_clockids)->default_value(false),
            "list all available clockids")
        ("command", po::value(&config.command));
    // clang-format on

    po::positional_options_description p;
    p.add("command", -1);

    po::variables_map vm;
    try
    {
        po::parsed_options parsed =
            po::command_line_parser(argc, argv).options(desc).positional(p).run();
        po::store(parsed, vm);
    }
    catch (const po::unknown_option& e)
    {
        std::cerr << e.what() << '\n';
        print_usage(std::cerr, argv[0], desc);
        std::exit(EXIT_FAILURE);
    }
    po::notify(vm);

    if (vm.count("help"))
    {
        print_usage(std::cout, argv[0], desc);
        std::exit(0);
    }

    if (quiet && verbosity.count != 0)
    {
        lo2s::Log::warn() << "Cannot be quiet and verbose at the same time. Refusing to be quiet.";
        quiet = false;
    }

    if (quiet)
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
    if (list_clockids)
    {
        std::cout << "Available clockids:\n";
        for (const auto& clock : lo2s::time::ClockProvider::get_descriptions())
        {
            std::cout << " * " << clock.name << '\n';
        }
        std::exit(EXIT_SUCCESS);
    }

    if (!perf::EventProvider::has_event(config.sampling_event))
    {
        lo2s::Log::error() << "requested sampling event \'" << config.sampling_event
                           << "\'is not available!";
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
        print_usage(std::cerr, argv[0], desc);
        std::exit(0);
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

    config.exclude_kernel = false;
    if (kernel && no_kernel)
    {
        lo2s::Log::warn() << "Cannot enable and disable kernel events at the same time.";
    }
    else if (no_kernel)
    {
        config.exclude_kernel = true;
    }

    instance = std::move(config);
}
}
