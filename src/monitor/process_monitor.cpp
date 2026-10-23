// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/process_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>
#include <lo2s/util.hpp>

#include <exception>
#include <tuple>
#include <utility>
#include <cassert>

namespace lo2s::monitor
{

ProcessMonitor::ProcessMonitor()
{
#ifdef HAVE_BPF
    if (config().perf.posix_io.enabled)
    {
        posix_monitor_ = std::make_unique<PosixMonitor>(trace_);
        posix_monitor_->start();
    }
#endif
    trace_.emplace_monitoring_thread(gettid(), "ProcessMonitor", "ProcessMonitor");
}

void ProcessMonitor::insert_process(Process parent, Process child, std::string proc_name,
                                    bool spawn)
{
    trace_.process_handle(parent, child, proc_name);
    insert_thread(child, child.as_thread(), proc_name, spawn);

    resolvers_.fork(parent, child);
}

void ProcessMonitor::insert_thread(Process parent [[maybe_unused]], Thread child, std::string name,
                                   bool spawn)
{
#ifdef HAVE_BPF
    if (posix_monitor_)
    {
        posix_monitor_->insert_thread(child);
    }
#endif
    trace_.thread_handle(parent, child, name);

    if (config().perf.sampling.enabled ||
        !perf::EventComposer::instance().has_group_counters_for(child) ||
        !perf::EventComposer::instance().has_userspace_counters_for(child))
    {
        try
        {
            const auto inserted =
                threads_.emplace(std::piecewise_construct, std::forward_as_tuple(child),
                                 std::forward_as_tuple(child, trace_, spawn));
            assert(inserted.second);
            // actually start thread
            inserted.first->second.start();
        }
        catch (const std::exception& e)
        {
            Log::warn() << "Could not start measurement for " << child << ": " << e.what();
        }
    }

}

void ProcessMonitor::update_process_name(Process process, const std::string& name)
{
    trace_.process_handle(Process::no_parent(), process, name);
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
} // namespace lo2s::monitor
