// SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

namespace lo2s::perf::counter
{
class MetricWriter
{
public:
    MetricWriter(MeasurementScope scope, trace::Trace& trace);

protected:
    time::Converter time_converter_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric metric_event_;
};
} // namespace lo2s::perf::counter
