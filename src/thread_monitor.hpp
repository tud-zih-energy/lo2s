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
#ifndef X86_RECORD_OTF2_THREAD_MONITOR_HPP
#define X86_RECORD_OTF2_THREAD_MONITOR_HPP

#include "otf2_counters.hpp"
#include "perf_sample_otf2.hpp"
#include "process_info.hpp"

#include <array>
#include <chrono>
#include <thread>

extern "C" {
#include <unistd.h>
}

namespace lo2s
{
class monitor;

class thread_monitor
{
public:
    using clock = std::chrono::high_resolution_clock;
    thread_monitor(pid_t pid, pid_t tid, monitor& parent_monitor, process_info& info,
                   bool enable_on_exec);
    ~thread_monitor();

    // We don't want copies. Should be implicitly deleted due to unique_ptr
    thread_monitor(const thread_monitor&) = delete;
    thread_monitor& operator=(const thread_monitor&) = delete;
    // Moving is still a bit tricky (keep moved-from in a useful state), avoid it for now.
    thread_monitor(thread_monitor&&) = delete;
    thread_monitor& operator=(thread_monitor&&) = delete;

    void disable();

    bool enabled() const
    {
        return enabled_;
    }

    bool finished() const
    {
        return finished_;
    }

private:
    void setup_thread();
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

    monitor& parent_monitor()
    {
        return parent_monitor_;
    }

    process_info& info()
    {
        return info_;
    }

private:
    pid_t pid_;
    pid_t tid_;
    monitor& parent_monitor_;
    process_info& info_;

    bool enabled_ = false;
    bool finished_ = false;
    size_t sample_rate = 1000000;
    cpu_set_t affinity_mask;
    int instructions_fd = -1;
    int cycles_fd = -1;
    std::array<int, 3> memory_fd{ { -1, -1, -1 } };

    std::thread thread;

    perf_sample_otf2 sample_reader_;
    otf2_counters counters_;
    std::chrono::nanoseconds read_interval_;
};
}

#endif // X86_RECORD_OTF2_THREAD_MONITOR_HPP
