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

#pragma once
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <otf2xx/otf2.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
class MetricWriter
{
public:
    MetricWriter(MeasurementScope scope, trace::Trace& trace)
    : time_converter_(time::Converter::instance()), writer_(trace.metric_writer(scope)),
      metric_instance_(trace.metric_instance(trace.perf_metric_class(scope), writer_.location(),
                                             trace.sample_writer(scope.scope).location())),
      metric_event_(otf2::chrono::genesis(), metric_instance_)
    {
    }

protected:
    time::Converter time_converter_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric metric_event_;
};
} // namespace counter
} // namespace perf
} // namespace lo2s
