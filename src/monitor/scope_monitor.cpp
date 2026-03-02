// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/scope_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/counter/group/writer.hpp>
#include <lo2s/perf/counter/userspace/writer.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/perf/syscall/writer.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <memory>

namespace lo2s::monitor
{

ScopeMonitor::ScopeMonitor(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec)
: PollMonitor(trace, scope.name()), scope_(scope)
{
    if (config().perf.sampling.enabled || config().perf.sampling.process_recording)
    {
        sample_writer_ = std::make_unique<perf::sample::Writer>(scope, trace, enable_on_exec);
        add_fd(sample_writer_->fd());
    }

    if (config().perf.syscall.enabled)
    {
        syscall_writer_ = std::make_unique<perf::syscall::Writer>(scope, trace);
        add_fd(syscall_writer_->fd());
    }

    if (config().perf.group.enabled)
    {
        group_counter_writer_ =
            std::make_unique<perf::counter::group::Writer>(scope, trace, enable_on_exec);
        add_fd(group_counter_writer_->fd());
    }

    if (config().perf.userspace.enabled)
    {
        userspace_counter_writer_ =
            std::make_unique<perf::counter::userspace::Writer>(scope, trace);
        add_fd(userspace_counter_writer_->fd());
    }

    // note: start() can now be called
}

void ScopeMonitor::initialize_thread()
{
    try_pin_to_scope(scope_);
}

void ScopeMonitor::finalize_thread()
{
    if (sample_writer_)
    {
        sample_writer_->end();
    }

    if (syscall_writer_)
    {
        syscall_writer_->stop();
    }
}

void ScopeMonitor::monitor(int fd)
{
    if (!scope_.is_cpu())
    {
        try_pin_to_scope(scope_);
    }

    if (syscall_writer_ && (fd == stop_pfd().fd || syscall_writer_->fd() == fd))
    {
        syscall_writer_->read();
    }
    if (sample_writer_ && (fd == stop_pfd().fd || sample_writer_->fd() == fd))
    {
        sample_writer_->read();
    }

    if (group_counter_writer_ && (fd == stop_pfd().fd || group_counter_writer_->fd() == fd))
    {
        group_counter_writer_->read();
    }
    if (userspace_counter_writer_ && (fd == stop_pfd().fd || userspace_counter_writer_->fd() == fd))
    {
        userspace_counter_writer_->read();
    }
}
} // namespace lo2s::monitor
