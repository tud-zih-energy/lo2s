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

namespace lo2s
{
namespace monitor
{

ProcessMonitor::ProcessMonitor() : MainMonitor()
{
#ifdef HAVE_BPF
    if (config().use_posix_io)
    {
        posix_monitor_ = std::make_unique<PosixMonitor>(trace_);
        posix_monitor_->start();
    }
#endif
    trace_.emplace_monitoring_thread(gettid(), "ProcessMonitor", "ProcessMonitor");
}

void ProcessMonitor::insert_process(Process parent, Process process, std::string proc_name,
                                    bool spawn)
{
    trace_.emplace_process(parent, process, proc_name);
    insert_thread(process, process.as_thread(), proc_name, spawn);

    resolvers_.fork(parent, process);
}

void ProcessMonitor::insert_thread(Process process [[maybe_unused]], Thread thread,
                                   std::string name, bool spawn)
{
#ifdef HAVE_BPF
    if (posix_monitor_)
    {
        posix_monitor_->insert_thread(thread);
    }
#endif
    trace_.emplace_thread(process, thread, name);

    if (config().use_perf_sampling)
    {
    }

    ExecutionScope scope = ExecutionScope(thread);
    if (config().use_perf_sampling ||
        !perf::EventComposer::instance().has_counters_for(MeasurementScope::group_metric(scope)) ||
        !perf::EventComposer::instance().has_counters_for(
            MeasurementScope::userspace_metric(scope)))
    {
        try
        {
            auto inserted =
                threads_.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                 std::forward_as_tuple(scope, trace_, spawn));
            assert(inserted.second);
            // actually start thread
            inserted.first->second.start();
        }
        catch (const std::exception& e)
        {
            Log::warn() << "Could not start measurement for " << thread << ": " << e.what();
        }
    }

    trace_.emplace_thread(process, thread, name);
}

void ProcessMonitor::update_process_name(Process process, const std::string& name)
{
    trace_.emplace_process(trace::NO_PARENT_PROCESS, process, name);
}

void ProcessMonitor::exit_thread(Thread thread)
{
#ifdef HAVE_BPF
    if (posix_monitor_)
    {
        posix_monitor_->exit_thread(thread);
    }
#endif
    if (threads_.count(thread) != 0)
    {
        threads_.at(thread).stop();
        threads_.at(thread).emplace_resolvers(resolvers_);
    }
    threads_.erase(thread);
}

ProcessMonitor::~ProcessMonitor()
{
    for (auto& thread : threads_)
    {
        thread.second.stop();
        thread.second.emplace_resolvers(resolvers_);
    }
#ifdef HAVE_BPF
    if (posix_monitor_)
    {
        posix_monitor_->stop();
    }
#endif
}
} // namespace monitor
} // namespace lo2s
