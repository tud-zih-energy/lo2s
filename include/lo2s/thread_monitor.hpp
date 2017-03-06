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

#pragma once

#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/trace/counters.hpp>

#include <array>
#include <chrono>
#include <thread>

#include <cstddef>

extern "C" {
#include <sched.h>
#include <unistd.h>
}

namespace lo2s
{
class Monitor;
class ProcessInfo;

class ThreadMonitor
{
public:
    using Clock = std::chrono::high_resolution_clock;
    ThreadMonitor(pid_t pid, pid_t tid, Monitor& parent_monitor, ProcessInfo& info,
                  bool enable_on_exec);
    ~ThreadMonitor();

    // We don't want copies. Should be implicitly deleted due to unique_ptr
    ThreadMonitor(const ThreadMonitor&) = delete;
    ThreadMonitor& operator=(const ThreadMonitor&) = delete;
    // Moving is still a bit tricky (keep moved-from in a useful state), avoid it for now.
    ThreadMonitor(ThreadMonitor&&) = delete;
    ThreadMonitor& operator=(ThreadMonitor&&) = delete;

    void stop();

    bool enabled() const
    {
        return enabled_.load();
    }

    bool finished() const
    {
        return finished_.load();
    }

private:
    void start();
    void run();
    void check_affinity(bool force = false);

public:
    pid_t pid() const
    {
        return pid_;
    }

    pid_t tid() const
    {
        return tid_;
    }

    Monitor& parent_monitor()
    {
        return parent_monitor_;
    }

    ProcessInfo& info()
    {
        return info_;
    }

private:
    pid_t pid_;
    pid_t tid_;
    Monitor& parent_monitor_;
    ProcessInfo& info_;

    std::atomic<bool> enabled_{ false };
    std::atomic<bool> finished_{ false };
    cpu_set_t affinity_mask_;

    std::thread thread_;

    perf::sample::Writer sample_writer_;
    trace::Counters counters_;
    std::chrono::nanoseconds read_interval_;
};
}
