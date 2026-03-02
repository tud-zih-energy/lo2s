// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/perf/counter/group/writer.hpp>

#include <lo2s/execution_scope.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/group/reader.hpp>
#include <lo2s/perf/counter/metric_writer.hpp>
#include <lo2s/trace/trace.hpp>

#include <cassert>
#include <cstddef>

namespace lo2s::perf::counter::group
{
Writer::Writer(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec)
: Reader(scope, enable_on_exec), MetricWriter(MeasurementScope::group_metric(scope), trace)
{
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    // update event timestamp from sample
    metric_event_.timestamp(time_converter_(sample->time));

    counter_buffer_.read(&sample->v);

    otf2::event::metric::values& values = metric_event_.raw_values();

    assert(counter_buffer_.size() <= values.size());

    // read counter values into metric event
    for (std::size_t i = 0; i < counter_buffer_.size(); i++)
    {
        values[i] = counter_buffer_[i] * counter_collection_.get_scale(i);
    }

    auto index = counter_buffer_.size();
    values[index++] = counter_buffer_.enabled();
    values[index++] = counter_buffer_.running();

    writer_.write(metric_event_);
    return false;
}

} // namespace lo2s::perf::counter::group
