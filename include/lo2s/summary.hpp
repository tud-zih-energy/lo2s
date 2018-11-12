/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_set>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{

class Summary
{
public:
    void show();

    void add_thread();
    void register_process(pid_t pid);

    void record_perf_wakeups(std::size_t num_wakeups);

    void set_exit_code(int exit_code);
    void set_trace_dir(const std::string& trace_dir);

    friend Summary& summary();

private:
    Summary();

    std::chrono::steady_clock::time_point start_wall_time_;

    std::atomic<std::size_t> num_wakeups_;
    std::atomic<std::size_t> thread_count_;

    std::unordered_set<pid_t> pids_;
    std::mutex pids_mutex_;

    std::string trace_dir_;

    int exit_code_;
};

Summary& summary();
} // namespace lo2s
