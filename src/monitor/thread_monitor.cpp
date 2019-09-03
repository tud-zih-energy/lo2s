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

#include <lo2s/monitor/thread_monitor.hpp>

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

ThreadMonitor::ThreadMonitor(pid_t pid, pid_t tid, ProcessMonitor& parent_monitor,
                             bool enable_on_exec)
: PollMonitor(parent_monitor.trace(), std::to_string(tid), config().perf_read_interval), pid_(pid),
  tid_(tid)
{
    if (config().sampling)
    {
        sample_writer_ = std::make_unique<perf::sample::Writer>(
            pid, tid, -1, parent_monitor, parent_monitor.trace(),
            parent_monitor.trace().thread_sample_writer(pid, tid), enable_on_exec);
        add_fd(sample_writer_->fd());
    }
    if (!perf::requested_events().events.empty())
    {
        counter_writer_ = std::make_unique<perf::counter::ProcessWriter>(
            pid, tid, parent_monitor.trace().thread_metric_writer(pid, tid), parent_monitor,
            enable_on_exec);
        add_fd(counter_writer_->fd());
    }

    /* setup the sampling counter(s) and start a monitoring thread */
    start();
}

void ThreadMonitor::stop()
{
    if (!thread_.joinable())
    {
        return;
    }

    stop_pipe_.write();
    thread_.join();
}

void ThreadMonitor::check_affinity(bool force)
{
    // Pin the monitoring thread on the same cores as the monitored thread
    cpu_set_t new_mask;
    CPU_ZERO(&new_mask); // make valgrind happy
    sched_getaffinity(tid(), sizeof(new_mask), &new_mask);
    if (force || !CPU_EQUAL(&new_mask, &affinity_mask_))
    {
        sched_setaffinity(0, sizeof(new_mask), &new_mask);
        affinity_mask_ = new_mask;
    }
}

void ThreadMonitor::initialize_thread()
{
    check_affinity(true);
}

void ThreadMonitor::finalize_thread()
{
    if (sample_writer_)
    {
        sample_writer_->end();
    }
}

void ThreadMonitor::monitor(int fd)
{
    check_affinity();

    if (sample_writer_ &&
        (fd == timer_pfd().fd || fd == stop_pfd().fd || sample_writer_->fd() == fd))
    {
        sample_writer_->read();
    }

    if (counter_writer_ &&
        (fd == timer_pfd().fd || fd == stop_pfd().fd || counter_writer_->fd() == fd))
    {
        counter_writer_->read();
    }
}
} // namespace monitor
} // namespace lo2s
