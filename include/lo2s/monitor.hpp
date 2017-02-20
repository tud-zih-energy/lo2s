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

#include <lo2s/metric/plugin/metrics.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/thread_map.hpp>

#include <otf2xx/definition/metric_class.hpp>

#include <memory>
#include <string>

extern "C" {
#include <signal.h>
}

namespace lo2s
{
namespace trace
{
class trace;
}
namespace perf
{
namespace tracepoint
{
class recorder;
}
}

class monitor
{
public:
    monitor(pid_t child, const std::string& name, trace::trace& trace, bool spawn,
            const monitor_config& config);

    ~monitor();

    void run();

    trace::trace& trace()
    {
        return trace_;
    }

    const perf::time::converter& time_converter() const
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
    perf::time::converter time_converter_;
    trace::trace& trace_;
    otf2::definition::metric_class counters_metric_class_;
    monitor_config config_;
    metric::plugin::metrics metrics_;
    std::unique_ptr<perf::tracepoint::recorder> raw_counters_;
};
}
