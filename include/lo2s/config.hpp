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
#include <string>
#include <vector>

#include <cstdint>

extern "C" {
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

struct Config
{
    // General
    MonitorType monitor_type;
    pid_t pid;
    std::vector<std::string> command;
    bool quiet;
    // Optional features
    std::vector<std::string> tracepoint_events;
    std::vector<std::string> perf_events;
#ifdef HAVE_X86_ADAPT
    std::vector<std::string> x86_adapt_cpu_knobs;
#endif
    // OTF2
    std::string trace_path;
    // perf
    std::size_t mmap_pages;
    bool exclude_kernel;
    // Instruction sampling
    std::uint64_t sampling_period;
    std::string sampling_event;
    bool enable_cct;
    bool suppress_ip;
    bool disassemble;
    // Interval monitors
    std::chrono::nanoseconds read_interval;
    // time synchronization
    bool use_clockid;
    clockid_t clockid;
};

const Config& config();
void parse_program_options(int argc, const char** argv);
}
