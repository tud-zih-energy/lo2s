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

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include <lo2s/types.hpp>

extern "C"
{
#include <unistd.h>
}

using namespace std::chrono_literals;

namespace lo2s
{
enum class MonitorType
{
    PROCESS,
    CPU_SET
};

enum class DwarfUsage
{
    NONE,
    LOCAL,
    FULL
};

struct Config
{
    // General
    bool quiet = false;
    MonitorType monitor_type;
    Process process;
    std::chrono::milliseconds read_interval;
    std::string lo2s_command_line;

    // OTF2
    std::string trace_path;

    // Program-under-test
    std::vector<std::string> command;

    bool drop_root = false;
    std::string user = "";

    // perf
    std::size_t mmap_pages;
    std::chrono::nanoseconds perf_read_interval = std::chrono::nanoseconds(0);
    int cgroup_fd = -1;

    // perf -- instruction sampling
    bool use_perf_sampling = false;
    bool use_process_recording = false;

    std::uint64_t perf_sampling_period;
    std::string perf_sampling_event;

    bool exclude_kernel = false;
    bool enable_callgraph = false;
    bool disassemble = false;

    DwarfUsage dwarf;

    // perf -- Python sampling
    bool use_python = true;

    // perf -- tracepoints
    std::vector<std::string> tracepoint_events;

    // perf -- block I/O
    bool use_block_io = false;

    // perf -- posix I/O
    bool use_posix_io = false;

    // perf -- syscall recording
    bool use_syscalls = false;
    std::vector<int64_t> syscall_filter;

    // perf -- group recorded metrics
    bool use_group_metrics = false;
    bool metric_use_frequency = true;

    std::uint64_t metric_count = 0;
    std::uint64_t metric_frequency = 0;

    std::string metric_leader = "";
    std::vector<std::string> group_counters;

    // perf -- userspace recorded metrics
    bool use_userspace_metrics = false;
    std::chrono::nanoseconds userspace_read_interval;
    std::vector<std::string> userspace_counters;

    // perf -- clock source
    std::optional<clockid_t> clockid = std::nullopt;
    bool use_pebs = false;

    // x86_energy
    bool use_x86_energy = false;

    // x86_adapt
    std::vector<std::string> x86_adapt_knobs;

    // Linux sensors
    bool use_sensors = false;

    // Accelerators
    // Accelerators -- NEC SX-Aurora Tsubasa
    bool use_nec = false;
    std::chrono::microseconds nec_read_interval;
    std::chrono::milliseconds nec_check_interval;

    // Accelerators -- Nvidia CUPTI
    bool use_nvidia = false;

    // Accelerators -- OpenMP
    bool use_openmp = false;

    // Accelerators -- AMD HIP
    bool use_hip = false;

    // Ringbuffer interface
    std::string socket_path;
    std::string injectionlib_path;
    uint64_t ringbuf_size;
    std::chrono::nanoseconds ringbuf_read_interval;

    bool use_any_tracepoint() const
    {
        return !tracepoint_events.empty() || use_block_io || use_syscalls;
    }

    bool use_perf() const
    {
        return use_block_io || use_group_metrics || use_userspace_metrics || use_perf_sampling ||
               use_process_recording;
    }
};

const Config& config();

const Config& config_or_default();

void parse_program_options(int argc, const char** argv);
} // namespace lo2s
