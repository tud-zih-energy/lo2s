/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016-2018, Technische Universitaet Dresden, Germany
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

#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/interval_monitor.hpp>

#ifdef HAVE_PERF
#include <lo2s/perf/counter/writer.hpp>
#include <lo2s/perf/sample/writer.hpp>
#endif

#include <lo2s/trace/counters.hpp>

#include <array>
#include <chrono>
#include <thread>

#include <cstddef>

extern "C"
{
#include <sched.h>
#include <unistd.h>
}

namespace lo2s
{
class ProcessInfo;

namespace monitor
{

class ThreadMonitor : public IntervalMonitor
{
public:
    ThreadMonitor(pid_t pid, pid_t tid, ProcessMonitor& parent_monitor, ProcessInfo& info,
                  bool enable_on_exec);

private:
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

    ProcessInfo& info()
    {
        return info_;
    }

    void initialize_thread() override;
    void finalize_thread() override;
    void monitor() override;

    std::string group() const override
    {
        return "lo2s::ThreadMonitor";
    }

private:
    pid_t pid_;
    pid_t tid_;
    ProcessInfo& info_;

#ifdef HAVE_LINUX
    cpu_set_t affinity_mask_;
#endif

#ifdef HAVE_PERF
    perf::sample::Writer sample_writer_;
    perf::counter::Writer counter_writer_;
#endif
};
} // namespace monitor
} // namespace lo2s
