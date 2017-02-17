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

#include <lo2s/metrics.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/thread_map.hpp>
#include <lo2s/time/converter.hpp>

#include <otf2xx/definition/metric_class.hpp>

#include <memory>
#include <string>

extern "C" {
#include <signal.h>
}

namespace lo2s
{
class otf2_trace;
class otf2_tracepoints;

class monitor
{
public:
    monitor(pid_t child, const std::string& name, otf2_trace& trace, bool spawn,
            const monitor_config& config);

    ~monitor();

    void run();

    otf2_trace& trace()
    {
        return trace_;
    }

    const perf_time_converter& time_converter() const
    {
        return time_converter_;
    }

    otf2::definition::metric_class counters_metric_class() const
    {
        return counters_metric_class_;
    }

    const monitor_config& config() const
    {
        return config_;
    }

private:
    void handle_ptrace_event_stop(pid_t child, int event);
    void handle_signal(pid_t child, int status);

    const pid_t first_child_;
    thread_map threads_;
    sighandler_t default_signal_handler;
    perf_time_converter time_converter_;
    otf2_trace& trace_;
    otf2::definition::metric_class counters_metric_class_;
    monitor_config config_;
    metrics metrics_;

    std::unique_ptr<otf2_tracepoints> raw_counters_;
};
}
