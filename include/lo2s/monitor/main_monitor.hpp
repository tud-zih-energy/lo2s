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
#ifdef HAVE_X86_ADAPT
#include <lo2s/metric/x86_adapt/metrics.hpp>
#endif
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{
class MetricMonitor;
}
} // namespace perf

namespace monitor
{

class MainMonitor
{
public:
    MainMonitor();

    virtual ~MainMonitor();

    trace::Trace& trace()
    {
        return trace_;
    }
    otf2::definition::metric_class get_metric_class()
    {
        return metric_class_;
    }

protected:
    otf2::definition::metric_class generate_metric_class();

    trace::Trace trace_;

    metric::plugin::Metrics metrics_;
    std::unique_ptr<perf::tracepoint::MetricMonitor> tracepoint_metrics_;
    otf2::definition::metric_class metric_class_;
#ifdef HAVE_X86_ADAPT
    std::unique_ptr<metric::x86_adapt::Metrics> x86_adapt_metrics_;
#endif
};
} // namespace monitor
} // namespace lo2s
