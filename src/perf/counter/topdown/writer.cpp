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

#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/topdown/writer.hpp>
#include <lo2s/time/time.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
namespace topdown
{
Writer::Writer(ExecutionScope scope, trace::Trace& trace)
: Reader(scope), time_converter_(time::Converter::instance()), writer_(trace.topdown_writer(scope)),
  metric_instance_(trace.metric_instance(trace.topdown_metric_class(), writer_.location(),
                                         trace.location(scope))),
  metric_event_(otf2::chrono::genesis(), metric_instance_)
{
}

bool Writer::handle(TopdownEvent* ev)
{
    metric_event_.timestamp(lo2s::time::now());
    otf2::event::metric::values& values = metric_event_.raw_values();

    values[0] = ev->slots;
    values[1] = ev->retiring;
    values[2] = ev->bad_spec;
    values[3] = ev->fe_bound;
    values[4] = ev->be_bound;

    writer_.write(metric_event_);
    return false;
}

} // namespace topdown
} // namespace counter
} // namespace perf
} // namespace lo2s
