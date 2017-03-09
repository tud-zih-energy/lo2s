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

#include <lo2s/thread_monitor.hpp>

#include <lo2s/log.hpp>
#include <lo2s/monitor.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/time/time.hpp>

#include <cassert>

namespace lo2s
{

ThreadMonitor::~ThreadMonitor()
{
    assert(!enabled());
    // Outside powers (i.e. ThreadMap) should make sure to not
    // destroy thread monitors that aren't finished.
    if (!finished())
    {
        Log::error() << "Trying to join non-finished_ thread monitor. That might take a while.";
    }
    thread_.join();
}

ThreadMonitor::ThreadMonitor(pid_t pid, pid_t tid, Monitor& parent_monitor, ProcessInfo& info,
                             bool enable_on_exec)
: pid_(pid), tid_(tid), parent_monitor_(parent_monitor), info_(info),
  sample_writer_(pid_, tid_, parent_monitor_.config(), *this, parent_monitor_.trace(),
                 parent_monitor_.time_converter(), enable_on_exec),
  counters_(pid, tid, parent_monitor_.trace(), parent_monitor_.counters_metric_class(),
            sample_writer_.location()),
  read_interval_(parent_monitor_.config().read_interval)
{
    (void)pid; // Unused
    /* setup the sampling counter(s) and start a monitoring thread */
    start();
}

void ThreadMonitor::stop()
{
    bool was_enabled = enabled_.exchange(false);
    if (!was_enabled)
    {
        Log::error() << "Trying to stop non-enabled_ ThreadMonitor.";
    }
    // Do not join here, it may take a while for the loop to finish
    // Instead join in the destructor
}

void ThreadMonitor::start()
{
    assert(!thread_.joinable());
    enabled_.store(true);
    thread_ = std::thread([this]() { this->run(); });
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

void ThreadMonitor::run()
{
    Log::info() << "New monitoring thread for: " << pid_ << "/" << tid_ << " with read interval of "
                << std::chrono::duration_cast<std::chrono::milliseconds>(read_interval_).count()
                << " ms";

    check_affinity(true);

    auto deadline = time::Clock::now();
    // Move deadline to be the same for all thread, reducing noise imbalances
    deadline -= (deadline.time_since_epoch() % read_interval_) + read_interval_;
    while (true)
    {
        Log::trace() << "Monitoring thread active";

        check_affinity();

        sample_writer_.read();
        counters_.write();

        if (!enabled())
        {
            break;
        }

        // TODO skip samples if we cannot keep up with the deadlines
        deadline += read_interval_;
        // TODO wait here for overflows in the sampling buffers in addition to the deadline!
        std::this_thread::sleep_until(deadline);
    }
    sample_writer_.end();
    Log::debug() << "Monitoring thread finished_";
    finished_.store(true);
}
}
