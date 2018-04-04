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
#include <lo2s/monitor/thread_monitor.hpp>
#include <lo2s/process_info.hpp>

#include <lo2s/process_map/process_map.hpp>
#include <lo2s/process_map/thread.hpp>

#include <memory>

namespace lo2s
{
namespace monitor
{

ProcessMonitor::ProcessMonitor() : MainMonitor()
{
    trace_.register_monitoring_tid(gettid(), "ProcessMonitor", "ProcessMonitor");
}

void ProcessMonitor::insert_first_process(pid_t pid, std::string proc_name, bool spawn)
{
    trace_.process(pid, proc_name);

    Process& process = process_map().get_process(pid);
    if (process.info == nullptr)
    {
        process.info = std::make_unique<ProcessInfo>(pid, spawn);
    }
    process_map().get_thread(pid).monitor =
        std::make_unique<ThreadMonitor>(pid, pid, *this, *process.info, spawn);
}

void ProcessMonitor::insert_process(pid_t pid, std::string proc_name)
{
    trace_.process(pid, proc_name);
    insert_thread(pid, pid);
}

void ProcessMonitor::insert_thread(pid_t pid, pid_t tid)
{
    Process& process = process_map().get_process(pid);
    if (process.info == nullptr)
    {
        process.info = std::make_unique<ProcessInfo>(pid, false);
    }
    process_map().get_thread(tid).monitor =
        std::make_unique<ThreadMonitor>(pid, tid, *this, *process.info, false);
}

void ProcessMonitor::exit_process(pid_t pid, std::string name)
{
    trace_.process(pid, name);
    exit_thread(pid);
}

void ProcessMonitor::exit_thread(pid_t tid)
{
    if (process_map().get_thread(tid).monitor != nullptr)
    {
        process_map().get_thread(tid).monitor->stop();
    }
}

ProcessMonitor::~ProcessMonitor()
{
    for (auto& thread : process_map().threads)
    {
        if (thread.second.monitor != nullptr)
        {
            thread.second.monitor->stop();
        }
    }
}
}
}
