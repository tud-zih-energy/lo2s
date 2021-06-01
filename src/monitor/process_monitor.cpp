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

void ProcessMonitor::insert_process(Process p, Process parent, std::string proc_name, bool spawn)
{
    trace_.add_process(p, parent, proc_name);
    insert_thread(p, p.as_thread(), proc_name, spawn);
}

void ProcessMonitor::insert_thread(Process p, Thread t, std::string name, bool spawn)
{
    trace_.add_thread(t, name);

    if (config().sampling)
    {
        process_infos_.emplace(std::piecewise_construct, std::forward_as_tuple(p),
                               std::forward_as_tuple(p, spawn));
    }

    if (config().sampling || !perf::counter::requested_group_counters().counters.empty() ||
        !perf::counter::requested_userspace_counters().counters.empty())
    {
        threads_.emplace(std::piecewise_construct, std::forward_as_tuple(t),
                         std::forward_as_tuple(ExecutionScope::thread(t), *this, spawn));
    }

    trace_.update_thread_name(t, name);
}

void ProcessMonitor::update_process_name(Process p, const std::string& name)
{
    trace_.update_process_name(p, name);
}

void ProcessMonitor::exit_process(Process p)
{
    exit_thread(p.as_thread());
}

void ProcessMonitor::exit_thread(Thread t)
{
    if (threads_.count(t) != 0)
    {
        threads_.at(t).stop();
    }
    threads_.erase(t);
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
