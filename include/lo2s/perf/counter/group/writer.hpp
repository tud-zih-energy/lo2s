// SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/execution_scope.hpp>
#include <lo2s/perf/counter/group/reader.hpp>
#include <lo2s/perf/counter/metric_writer.hpp>
#include <lo2s/trace/fwd.hpp>

namespace lo2s::perf::counter::group
{
class Writer : public Reader<Writer>, MetricWriter
{
public:
    Writer(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec);

    using Reader<Writer>::handle;
    bool handle(const RecordSampleType* sample);
};
} // namespace lo2s::perf::counter::group
