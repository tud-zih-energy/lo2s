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
#include <lo2s/perf/util.hpp>

#include <nitro/lang/optional.hpp>

#include <boost/program_options.hpp>

#include <cstdlib>

namespace po = boost::program_options;

namespace lo2s
{
static nitro::lang::optional<Config> instance;

const Config& config()
{
    return *instance;
}

void parse_program_options(int argc, const char** argv)
{
    po::options_description desc("Allowed options");

    lo2s::Config config;
    bool quiet;
    bool debug;
    bool trace;
    bool all_cpus;
    bool disassemble, no_disassemble;
    bool kernel, no_kernel;
    std::uint64_t read_interval_ms;

    // TODO read default for mmap-pages from (/proc/sys/kernel/perf_event_mlock_kb / pagesize) - 1
    // clang-format off
    desc.add_options()
        ("help",
             "produce help message")
        ("output-trace,o", po::value(&config.trace_path),
             "output trace directory")
        ("sampling_period,s", po::value(&config.sampling_period)->default_value(11010113),
             "sampling period (# instructions)")
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
        ("verbose,v", po::bool_switch(&debug),
             "verbose output")
        ("extra-verbose,t", po::bool_switch(&trace),
             "extra verbose output")
        ("mmap-pages,m", po::value(&config.mmap_pages)->default_value(16),
             "number of pages to be used by each internal buffer")
        ("readout-interval,i", po::value(&read_interval_ms)->default_value(100),
             "time interval between metric and sampling buffer readouts in milliseconds")
        ("raw-tracepoint-event,e", po::value(&config.tracepoint_events),
             "enable global recording of a raw tracepoint event (usually requires root)")
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
        ("command", po::value(&config.command));
    // clang-format on

    po::positional_options_description p;
    p.add("command", -1);

    po::variables_map vm;
    po::parsed_options parsed =
        po::command_line_parser(argc, argv).options(desc).positional(p).run();
    po::store(parsed, vm);
    po::notify(vm);

    if (all_cpus)
    {
        config.monitor_type = lo2s::MonitorType::CPU_SET;
    }
    else
    {
        config.monitor_type = lo2s::MonitorType::PROCESS;
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

    if (vm.count("help") || (config.monitor_type == lo2s::MonitorType::PROCESS &&
                             config.pid == -1 && config.command.empty()))
    {
        std::cout << desc << "\n";
        std::exit(0);
    }

    if (trace)
    {
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::trace);
        lo2s::Log::trace() << "Enabling log-level 'trace'";
    }
    else if (debug)
    {
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::debug);
        lo2s::Log::debug() << "Enabling log-level 'debug'";
    }
    else if (quiet)
    {
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::warn);
    }
    else
    {
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::info);
    }

    instance = std::move(config);
}
}
