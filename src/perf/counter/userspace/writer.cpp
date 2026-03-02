// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
