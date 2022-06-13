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
#include <lo2s/perf/counter/userspace/writer.hpp>
#include <lo2s/time/time.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
namespace userspace
{
Writer::Writer(ExecutionScope scope, trace::Trace& trace)
: Reader(scope), MetricWriter(MeasurementScope::userspace_metric(scope), trace),
  counters_(requested_userspace_counters())
{
}

bool Writer::handle(std::vector<UserspaceReadFormat>& data)
{
    // update event timestamp from sample
    metric_event_.timestamp(lo2s::time::now());

    counter_buffer_.read(data);

    otf2::event::metric::values& values = metric_event_.raw_values();

    assert(counter_buffer_.size() <= values.size());

    // read counter values into metric event

    for (std::size_t i = 0; i < counter_buffer_.size(); i++)
    {
        values[i] = counter_buffer_[i] * counters_.get_scale(i);
    }

    writer_.write(metric_event_);
    return false;
}

} // namespace userspace
} // namespace counter
} // namespace perf
} // namespace lo2s
