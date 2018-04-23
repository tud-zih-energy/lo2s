/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <lo2s/log.hpp>
#include <lo2s/perf/counter/writer.hpp>
#include <lo2s/perf/event_collection.hpp>
#include <lo2s/time/time.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
Writer::Writer(pid_t pid, pid_t tid, trace::Trace& trace, otf2::definition::location scope,
               bool enable_on_exec)
: Reader(tid, requested_events(), enable_on_exec),
  counter_writer_(pid, tid, trace, scope),
  time_converter_(time::Converter::instance())
{
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);

    counters_.read(&sample->v);
    counter_writer_.write(counters_, tp);
    return false;
}
}
}
}
