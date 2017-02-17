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

#include <lo2s/tracepoint/event_format.hpp>
#include <lo2s/tracepoint/perf_tracepoint_reader.hpp>

#include <lo2s/monitor_config.hpp>
#include <lo2s/otf2_trace.hpp>
#include <lo2s/time/converter.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <vector>

namespace lo2s
{

class perf_tracepoint_otf2 : public perf_tracepoint_reader<perf_tracepoint_otf2>
{
public:
    perf_tracepoint_otf2(int cpu, const event_format& event, const monitor_config& config,
                         otf2_trace& trace, const otf2::definition::metric_class& metric_class,
                         const perf_time_converter& time_converter);

    perf_tracepoint_otf2(const perf_tracepoint_otf2& other) = delete;

    perf_tracepoint_otf2(perf_tracepoint_otf2&& other) = default;

public:
    using perf_tracepoint_reader<perf_tracepoint_otf2>::handle;
    bool handle(const perf_tracepoint_reader::record_sample_type* sample);

private:
    event_format event_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;

    const perf_time_converter& time_converter_;

    std::vector<otf2::event::metric::value_container> counter_values_;
};
}
