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
#include <lo2s/process_info.hpp>

namespace lo2s
{
namespace monitor
{

ProcessMonitor::ProcessMonitor() : MainMonitor()
{
    trace_.add_monitoring_thread(gettid(), "ProcessMonitor", "ProcessMonitor");
}

void ProcessMonitor::insert_process(Process process, Thread parent, std::string proc_name,
                                    bool spawn)
{
    trace_.add_process(process, parent, proc_name);
    insert_thread(process.as_thread(), process, proc_name, spawn);
}

void ProcessMonitor::insert_thread(Thread thread, Process parent, std::string name, bool spawn)
{
    trace_.add_thread(thread, name);

    if (config().sampling)
    {
        process_infos_.emplace(std::piecewise_construct, std::forward_as_tuple(parent),
                               std::forward_as_tuple(parent, spawn));
    }

    if (config().sampling || !perf::counter::requested_group_counters().counters.empty() ||
        !perf::counter::requested_userspace_counters().counters.empty())
    {
        threads_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                         std::forward_as_tuple(ExecutionScope::thread(thread), *this, spawn));
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
