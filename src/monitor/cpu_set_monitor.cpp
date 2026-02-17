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

#include <lo2s/monitor/cpu_set_monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/monitor/process_monitor_main.hpp>
#include <lo2s/monitor/system_process_monitor.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#include <filesystem>
#include <regex>

#include <csignal>

namespace lo2s
{
namespace monitor
{
CpuSetMonitor::CpuSetMonitor() : MainMonitor()
{
    trace_.emplace_monitoring_thread(gettid(), "CpuSetMonitor", "CpuSetMonitor");

    // Prefill Memory maps
    std::regex proc_regex("/proc/([0-9]+)");
    std::smatch pid_match;
    pid_t pid;

    const std::filesystem::path proc_path("/proc");
    if (config().use_perf_sampling)
    {
        for (const auto& p : std::filesystem::directory_iterator(proc_path))
        {
            std::string path = p.path().string();
            if (std::regex_match(path, pid_match, proc_regex))
            {
                pid = std::stol(pid_match[1]);

                auto maps = read_maps(Process(pid));
                Process p(pid);
                resolvers_.function_resolvers.emplace(
                    std::piecewise_construct, std::forward_as_tuple(p), std::forward_as_tuple(p));
                for (auto& map : maps)
                {
                    resolvers_.emplace_mappings_for(p, map.first, map.second);
                }
            }
        }
    }

    if (config().use_perf_sampling || config().use_process_recording)
    {
        trace_.emplace_threads(get_comms_for_running_threads());
    }

    try
    {
        for (const auto& cpu : Topology::instance().cpus())
        {
            Log::debug() << "Create cstate recorder for cpu #" << cpu.as_int();

            auto inserted =
                monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(cpu),
                                  std::forward_as_tuple(ExecutionScope(cpu), trace_, false));
            assert(inserted.second);
            // directly start the measurement thread
            inserted.first->second.start();
        }
    }
    catch (...)
    {
        Log::error() << "Failed to create/start all CPU monitors (" << monitors_.size()
                     << " out of " << Topology::instance().cpus().size()
                     << " suceeded): remove already existing monitors";

        // clean up existing thread
        for (auto& monitor_map_it : monitors_)
        {
            monitor_map_it.second.stop();
        }

        throw;
    }
}

void CpuSetMonitor::run()
{
    sigset_t ss;
    if (config().command.empty() && config().process == Process::invalid())
    {
        sigemptyset(&ss);
        sigaddset(&ss, SIGINT);

        auto ret = pthread_sigmask(SIG_BLOCK, &ss, NULL);
        if (ret)
        {
            Log::error() << "Failed to set pthread_sigmask: " << ret;
            throw std::runtime_error("Failed to set pthread_sigmask");
        }
    }

    if (config().command.empty() && config().process == Process::invalid())
    {
        int sig;
        auto ret = sigwait(&ss, &sig);
        std::cout << "[ lo2s: Encountered SIGINT. Stopping measurements and closing trace ]"
                  << std::endl;
        if (ret)
        {
            throw make_system_error();
        }
    }
    else
    {
        try
        {
            SystemProcessMonitor monitor(trace_);
            process_monitor_main(monitor);
        }
        catch (std::system_error& e)
        {
            if (e.code())
            {
                throw e;
            }
        }
    }

    if (config().use_perf_sampling || config().use_process_recording)
    {
        trace_.emplace_threads(get_comms_for_running_threads());
    }

    for (auto& monitor_elem : monitors_)
    {
        monitor_elem.second.stop();
        monitor_elem.second.emplace_resolvers(resolvers_);
    }

    throw std::system_error(0, std::system_category());
}
} // namespace monitor
} // namespace lo2s
