/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2022,
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

#include <lo2s/perf/syscall/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/tracepoint/format.hpp>

#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <set>

namespace lo2s
{
namespace perf
{
namespace syscall
{
// Note, this cannot be protected for CRTP reasons...
class Writer : public Reader<Writer>
{
public:
    Writer(ExecutionScope scope, trace::Trace& trace);

    Writer(const Writer& other) = delete;

    Writer(Writer&& other) = default;

    ~Writer();

    using Reader<Writer>::handle;

    bool handle(const Reader::RecordSampleType* sample);

private:
    static constexpr int CCTX_LEVEL_SYSCALL = 1;

    LocalCctxTree& local_cctx_tree_;
    const time::Converter& time_converter_;

    otf2::chrono::time_point last_tp_;
};
} // namespace syscall
} // namespace perf
} // namespace lo2s
