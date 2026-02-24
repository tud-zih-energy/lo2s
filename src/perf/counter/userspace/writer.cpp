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

#include <lo2s/perf/counter/userspace/writer.hpp>

#include <lo2s/execution_scope.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/metric_writer.hpp>
#include <lo2s/perf/counter/userspace/reader.hpp>
#include <lo2s/perf/counter/userspace/userspace_counter_buffer.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>

#include <vector>

#include <cassert>
#include <cstddef>

namespace lo2s::perf::counter::userspace
{
Writer::Writer(ExecutionScope scope, trace::Trace& trace)
: Reader(scope), MetricWriter(MeasurementScope::userspace_metric(scope), trace)
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
        // In get_scale, index 0 is reserved for the metric leader, which we don't have in the
        // userspace metric mode, so add 1 to the counter index
        values[i] = counter_buffer_[i] * counter_collection_.get_scale(i + 1);
    }

    writer_.write(metric_event_);
    return false;
}

} // namespace lo2s::perf::counter::userspace
