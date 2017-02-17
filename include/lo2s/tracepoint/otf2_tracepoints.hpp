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

#include "perf_tracepoint_otf2.hpp"

#include <lo2s/monitor_config.hpp>
#include <lo2s/otf2_trace.hpp>
#include <lo2s/pipe.hpp>
#include <lo2s/time/time.hpp>

#include <thread>

namespace lo2s
{
/*
 * NOTE: Encapsulates counters for ALL cpus, totally different than otf2_counters!
 */
class otf2_tracepoints
{
public:
    otf2_tracepoints(otf2_trace& trace, const monitor_config& config,
                      const perf_time_converter& time_converter);
    ~otf2_tracepoints();

public:
    void stop();

private:
    void start();
    void poll();
    void stop_all();

private:
    pipe stop_pipe_;
    std::vector<perf_tracepoint_otf2> perf_recorders_;
    std::thread thread_;
};
}
