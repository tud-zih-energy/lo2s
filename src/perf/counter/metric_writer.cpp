// SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/perf/counter/metric_writer.hpp>

#include <lo2s/measurement_scope.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/chrono/time_point.hpp>

namespace lo2s::perf::counter
{
MetricWriter::MetricWriter(MeasurementScope scope, trace::Trace& trace)
: time_converter_(time::Converter::instance()), writer_(trace.metric_writer(scope)),
  metric_instance_(
      trace.metric_instance(trace.perf_metric_class(scope), writer_.location(),
                            trace.sample_writer(MeasurementScope::sample(scope.scope)).location())),
  metric_event_(otf2::chrono::genesis(), metric_instance_)
{
}
} // namespace lo2s::perf::counter
