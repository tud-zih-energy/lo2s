// SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/execution_scope.hpp>
#include <lo2s/perf/counter/metric_writer.hpp>
#include <lo2s/perf/counter/userspace/reader.hpp>
#include <lo2s/perf/counter/userspace/userspace_counter_buffer.hpp>
#include <lo2s/trace/fwd.hpp>

#include <vector>

namespace lo2s::perf::counter::userspace
{
class Writer : public Reader<Writer>, MetricWriter
{
public:
    Writer(ExecutionScope scope, trace::Trace& trace);

    bool handle(std::vector<UserspaceReadFormat>& data);
};
} // namespace lo2s::perf::counter::userspace
