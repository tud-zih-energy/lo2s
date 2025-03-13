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
    MonitorType monitor_type;
    Process process;
    std::vector<std::string> command;
    std::string command_line;
    bool quiet = false;
    bool drop_root = false;
    std::string user = "";
    // Optional features
    std::vector<std::string> tracepoint_events;
#ifdef HAVE_X86_ADAPT
    std::vector<std::string> x86_adapt_knobs;
#endif
    bool use_sensors = false;
    int cgroup_fd = -1;
    // OTF2
    std::string trace_path;
    // perf
    std::size_t mmap_pages;
    bool exclude_kernel = false;
    bool process_recording = false;
    // Instruction sampling
    bool sampling = false;
    std::uint64_t sampling_period;
    std::string sampling_event;
    bool enable_cct = false;
    bool suppress_ip = false;
    bool disassemble = false;
    // Interval monitors
    std::chrono::nanoseconds read_interval;
    std::chrono::nanoseconds userspace_read_interval;
    std::chrono::nanoseconds perf_read_interval = std::chrono::nanoseconds(0);
    std::chrono::nanoseconds ringbuf_read_interval;
    // Metrics
    bool metric_use_frequency = true;

    std::uint64_t metric_count;
    std::uint64_t metric_frequency;

    std::string metric_leader;
    std::vector<std::string> group_counters;
    std::vector<std::string> userspace_counters;
    // time synchronizatio
    std::optional<clockid_t> clockid;
    bool use_pebs = false;
    // x86_energy
    bool use_x86_energy = false;
    // block I/O
    bool use_block_io = false;
    // posix I/O
    bool use_posix_io = false;
    // syscalls
    bool use_syscalls = false;
    std::vector<int64_t> syscall_filter;
    // NEC SX-Aurora Tsubasa
    bool use_nec = false;
    std::chrono::microseconds nec_read_interval;
    std::chrono::milliseconds nec_check_interval;
    // Nvidia CUPTI
    bool use_nvidia = false;
    // Function resolution
    DwarfUsage dwarf;
    // Ringbuffer interface
    std::string socket_path;
    std::string injectionlib_path;
    uint64_t ringbuf_size;
};

const Config& config();
void parse_program_options(int argc, const char** argv);
} // namespace lo2s
