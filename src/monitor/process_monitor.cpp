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

#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/monitor/scope_monitor.hpp>
#include <lo2s/perf/counter/counter_provider.hpp>
#include <lo2s/process_info.hpp>

namespace lo2s
{
namespace monitor
{

ProcessMonitor::ProcessMonitor() : MainMonitor()
{
    trace_.add_monitoring_thread(gettid(), "ProcessMonitor", "ProcessMonitor");
}

void ProcessMonitor::insert_process(Process parent, Process process, std::string proc_name,
                                    bool spawn)
{
    trace_.add_process(parent, process, proc_name);
    insert_thread(process, process.as_thread(), proc_name, spawn);
}

void ProcessMonitor::insert_thread(Process process, Thread thread, std::string name, bool spawn)
{
    trace_.add_thread(thread, name);

    if (config().sampling)
    {
        process_infos_.try_emplace(process, process, spawn);
    }

    if (config().sampling ||
        perf::counter::CounterProvider::instance().has_group_counters(ExecutionScope(thread)) ||
        perf::counter::CounterProvider::instance().has_userspace_counters(ExecutionScope(thread)))
    {
        try
        {
            auto inserted =
                threads_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                 std::forward_as_tuple(ExecutionScope(thread), *this, spawn));
            assert(inserted.second);
            // actually start thread
            inserted.first->second.start();
        }
        catch (const std::exception& e)
        {
            Log::warn() << "Could not start measurement for " << thread << ": " << e.what();
        }
    }

    trace_.update_thread_name(thread, name);
}

void ProcessMonitor::update_process_name(Process process, const std::string& name)
{
    trace_.update_process_name(process, name);
}

void ProcessMonitor::exit_thread(Thread thread)
{
    if (threads_.count(thread) != 0)
    {
        threads_.at(thread).stop();
    }
    threads_.erase(thread);
}

ProcessMonitor::~ProcessMonitor()
{
    for (auto& thread : threads_)
    {
        thread.second.stop();
    }
}
} // namespace monitor
} // namespace lo2s
