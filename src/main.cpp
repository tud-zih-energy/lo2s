/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/pipe.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <boost/program_options.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <sys/prctl.h>
#include <sys/ptrace.h>
}
namespace po = boost::program_options;

namespace lo2s
{
static void run_command(const std::vector<std::string>& command_and_args, Pipe& go_pipe)
{
    /* kill yourself if the parent dies */
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    /* we need ptrace to get fork/clone/... */
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);

    std::vector<char*> tmp;
    std::transform(command_and_args.begin(), command_and_args.end(), std::back_inserter(tmp),
                   [](const std::string& s) {
                       char* pc = new char[s.size() + 1];
                       std::strcpy(pc, s.c_str());
                       return pc;
                   });
    tmp.push_back(nullptr);

    Log::debug() << "execve(" << command_and_args[0] << ")";

    // Wait until we are allowed to execve
    auto ret = go_pipe.read();

    if (ret != 1)
    {
        Log::info() << "Measurement aborted prematurely, will not run the command";
        exit(0);
    }

    // Stop yourself so the parent tracer can do initialize the options
    raise(SIGSTOP);

    // run the application which should be sampled
    execvp(tmp[0], &tmp[0]);

    // should not be executed -> exec failed, let's clean up anyway.
    for (auto cp : tmp)
    {
        delete[] cp;
    }
    Log::error() << "Could not execute command: " << command_and_args[0];
    throw_errno();
}

void setup_measurement(const std::vector<std::string>& command_and_args, pid_t pid,
                       const MonitorConfig& config, const std::string& trace_path)
{
    std::unique_ptr<Pipe> child_ready_pipe;
    std::unique_ptr<Pipe> go_pipe;
    assert(pid != 0);
    bool spawn = (pid == -1);
    if (spawn)
    {
        child_ready_pipe = std::make_unique<Pipe>();
        go_pipe = std::make_unique<Pipe>();
        pid = fork();
    }
    else
    {
        // TODO Attach to all threads in a process
        if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1)
        {
            Log::error() << "Could not attach to pid " << pid
                         << ". Try setting /proc/sys/kernel/yama/ptrace_scope to 0";
            throw_errno();
        }
    }
    if (pid == -1)
    {
        Log::error() << "Fork failed.";
        throw_errno();
    }
    else if (pid == 0)
    {
        assert(spawn);
        child_ready_pipe->close_read_fd();
        go_pipe->close_write_fd();
        go_pipe->read_fd_flags(FD_CLOEXEC);

        // Tell the parent we're ready to go
        child_ready_pipe->close_write_fd();

        run_command(command_and_args, *go_pipe);
    }
    else
    {
        if (spawn)
        {
            child_ready_pipe->close_write_fd();
            go_pipe->close_read_fd();
        }
        trace::Trace trace_(config.sampling_period, trace_path);

        std::string proc_name;
        if (spawn)
        {
            // Wait for child
            child_ready_pipe->read();

            // No idea why we do this... perf does it
            go_pipe->write_fd_flags(FD_CLOEXEC);
            child_ready_pipe->close_read_fd();
            proc_name = command_and_args[0];
        }
        else
        {
            proc_name = get_process_exe(pid);
        }

        monitor::ProcessMonitor m(pid, proc_name, trace_, spawn, config);

        if (spawn)
        {
            // Signal child that it can exec
            go_pipe->write();
        }

        m.run();
    }
}
}

int main(int argc, const char** argv)
{
    pid_t pid = -1;
    std::string output_trace("");
    po::options_description desc("Allowed options");
    std::vector<std::string> command;
    lo2s::MonitorConfig config;
    bool quiet;
    bool debug;
    bool trace;
    // clang-format off
    std::uint64_t read_interval_ms;

    // TODO read default for mmap-pages from (/proc/sys/kernel/perf_event_mlock_kb / pagesize) - 1
    desc.add_options()
            ("help",
                 "produce help message")
            ("output-trace,o", po::value(&output_trace),
                 "output trace directory")
            ("sampling_period,s", po::value(&config.sampling_period)->default_value(11010113),
                 "sampling period (# instructions)")
            ("call-graph,g", po::bool_switch(&config.enable_cct)->default_value(false),
                 "call-graph recording")
            ("no-ip,n", po::bool_switch(&config.suppress_ip)->default_value(false),
                 "do not record instruction pointers [NOT CURRENTLY SUPPORTED]")
            ("pid,p", po::value(&pid),
                 "attach to specific pid")
            ("quiet,q", po::bool_switch(&quiet)->default_value(false),
                 "suppress output")
            ("verbose,v", po::bool_switch(&debug)->default_value(false),
                 "verbose output")
            ("extra-verbose,t", po::bool_switch(&trace)->default_value(false),
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
            ("command", po::value(&command));
    // clang-format on

    po::positional_options_description p;
    p.add("command", -1);

    po::variables_map vm;
    po::parsed_options parsed =
        po::command_line_parser(argc, argv).options(desc).positional(p).run();
    po::store(parsed, vm);
    po::notify(vm);

    config.read_interval = std::chrono::milliseconds(read_interval_ms);

    if (vm.count("help") || (pid == -1 && command.empty()))
    {
        std::cout << desc << "\n";
        return 0;
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

    try
    {
        lo2s::setup_measurement(command, pid, config, output_trace);
    }
    catch (const std::system_error& e)
    {
        // Check for success
        if (e.code())
        {
            lo2s::Log::error() << "Aborting: " << e.what();
        }
        return e.code().value();
    }

    return 0;
}
