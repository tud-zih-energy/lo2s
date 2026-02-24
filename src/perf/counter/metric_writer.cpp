/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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
