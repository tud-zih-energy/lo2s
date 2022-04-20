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

#include <lo2s/monitor/scope_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/time/time.hpp>

#include <memory>
#include <string>

#include <cassert>

extern "C"
{
#include <sched.h>
}

namespace lo2s
{
namespace monitor
{

ScopeMonitor::ScopeMonitor(ExecutionScope scope, MainMonitor& parent, bool enable_on_exec)
: PollMonitor(parent.trace(), scope.name(), config().perf_read_interval), scope_(scope)
{
#ifndef USE_PERF_RECORD_SWITCH
    if (config().sampling)
#else
    if (config().sampling || scope.is_cpu())
#endif
    {
        sample_writer_ =
            std::make_unique<perf::sample::Writer>(scope, parent, parent.trace(), enable_on_exec);
        add_fd(sample_writer_->fd());
    }

    if (!perf::counter::requested_group_counters().counters.empty())
    {
        group_counter_writer_ =
            std::make_unique<perf::counter::group::Writer>(scope, parent.trace(), enable_on_exec);
        add_fd(group_counter_writer_->fd());
    }

    if (!perf::counter::requested_userspace_counters().counters.empty())
    {
        userspace_counter_writer_ =
            std::make_unique<perf::counter::userspace::Writer>(scope, parent.trace());
        add_fd(userspace_counter_writer_->fd());
    }

#ifndef USE_PERF_RECORD_SWITCH
    if (scope.is_cpu())
    {
        switch_writer_ =
            std::make_unique<perf::tracepoint::SwitchWriter>(scope.as_cpu(), parent.trace());
    }
#endif

    /* setup the sampling counter(s) and start a monitoring thread */
    start();
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
}

void ScopeMonitor::monitor(int fd)
{
    if (!scope_.is_cpu())
    {
        try_pin_to_scope(scope_);
    }

    if (sample_writer_ &&
        (fd == timer_pfd().fd || fd == stop_pfd().fd || sample_writer_->fd() == fd))
    {
        sample_writer_->read();
    }

    if (group_counter_writer_ &&
        (fd == timer_pfd().fd || fd == stop_pfd().fd || group_counter_writer_->fd() == fd))
    {
        group_counter_writer_->read();
    }
    if (userspace_counter_writer_ &&
        (fd == timer_pfd().fd || fd == stop_pfd().fd || userspace_counter_writer_->fd() == fd))
    {
        userspace_counter_writer_->read();
    }
#ifndef USE_PERF_RECORD_SWITCH
    if (switch_writer_ && (fd == timer_pfd().fd || fd == stop_pfd().fd))
    {
        switch_writer_->read();
    }
#endif
}
} // namespace monitor
} // namespace lo2s
