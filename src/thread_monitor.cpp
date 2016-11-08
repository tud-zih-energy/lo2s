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
#include "thread_monitor.hpp"
#include "log.hpp"
#include "mem_event.h"
#include "mem_levels.h"
#include "monitor.hpp"
#include "monitor_config.hpp"
#include "perf_sample_otf2.hpp"

#include <memory>

#include <cstring>
extern "C" {
#include <fcntl.h>
#include <sys/signal.h>
}

namespace lo2s
{

thread_monitor::~thread_monitor()
{
    assert(!enabled_);
    if (!finished_)
    {
        log::warn() << "Trying to join non-finished_ thread monitor. That should not happen.";
    }
    thread.join();
}

thread_monitor::thread_monitor(pid_t pid, pid_t tid, monitor& parent_monitor, process_info& info,
                               bool enable_on_exec)
: pid_(pid), tid_(tid), parent_monitor_(parent_monitor), info_(info),
  sample_reader_(pid_, tid_, parent_monitor_.config(), *this, parent_monitor_.trace(),
                 parent_monitor_.time_converter(), enable_on_exec),
  counters_(pid, tid, parent_monitor_.trace(), parent_monitor_.counters_metric_class(),
            sample_reader_.location())
{
    (void)pid; // Unused
    /* setup the sampling counter(s) and start a monitoring thread */
    setup_thread();
}

void thread_monitor::disable()
{
    if (!enabled_)
    {
        log::warn() << "Trying to disable non-enabled_ thread_monitor. This should not happen.";
    }
    enabled_ = false;
}

void thread_monitor::setup_thread()
{
    enabled_ = true;
    thread = std::thread([this]() { this->run(); });
}

void thread_monitor::check_affinity(bool force)
{
    // Pin the monitoring thread on the same cores as the monitored thread
    cpu_set_t new_mask;
    CPU_ZERO(&new_mask); // make valgrind happy
    sched_getaffinity(tid(), sizeof(new_mask), &new_mask);
    if (force || !CPU_EQUAL(&new_mask, &affinity_mask))
    {
        sched_setaffinity(0, sizeof(new_mask), &new_mask);
        affinity_mask = new_mask;
    }
}

void thread_monitor::run()
{
    log::info() << "New monitoring thread for: " << pid_ << "/" << tid_;

    check_affinity(true);

    auto deadline = clock::now();
    // Move deadline to be the same for all thread, reducing noise imbalances
    deadline -= (deadline.time_since_epoch() % read_interval) + read_interval;
    while (true)
    {
        log::trace() << "Monitoring thread active";

        check_affinity();

        sample_reader_.read();
        counters_.write();

        if (!enabled_)
        {
            break;
        }

        deadline += read_interval;
        std::this_thread::sleep_until(deadline);
    }
    sample_reader_.writer() << otf2::event::thread_end(get_time(),
                                                       parent_monitor_.trace().self_comm(), -1);
    //    sample_reader_.writer() << otf2::event::thread_team_end(get_time(),
    //                                                            parent_monitor_.trace().self_comm());
    log::debug() << "Monitoring thread finished_";
    finished_ = true;
}
}
