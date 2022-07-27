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

#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/counter/metric_writer.hpp>
#include <lo2s/perf/counter/userspace/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <vector>
namespace lo2s
{
namespace perf
{
namespace counter
{
namespace userspace
{
class Writer : public Reader<Writer>, MetricWriter
{
public:
    Writer(ExecutionScope scope, trace::Trace& trace);

    bool handle(std::vector<UserspaceReadFormat>& data);

private:
    const CounterCollection& counters_;
};
} // namespace userspace
} // namespace counter
} // namespace perf
} // namespace lo2s
