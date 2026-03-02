// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/types/process.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <string>

#include <cstddef>

namespace lo2s
{

class Summary
{
public:
    void show();

    void add_thread();
    void register_process(Process process);

    void record_perf_wakeups(std::size_t num_wakeups);

    void set_exit_code(int exit_code);
    void set_trace_dir(const std::string& trace_dir);

    friend Summary& summary();

private:
    Summary();

    std::chrono::steady_clock::time_point start_wall_time_;

    std::atomic<std::size_t> num_wakeups_;
    std::atomic<std::size_t> thread_count_;

    std::set<Process> processes_;
    std::mutex processes_mutex_;

    std::string trace_dir_;

    int exit_code_{ 0 };
};

Summary& summary();
} // namespace lo2s
