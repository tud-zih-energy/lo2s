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
#include <lo2s/time/time.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
Writer::Writer(pid_t pid, pid_t tid, trace::Trace& trace,
               otf2::definition::metric_class metric_class, otf2::definition::location scope)
: Reader(tid, trace::Counters::collect_counters()),
  counter_writer_(pid, tid, trace, metric_class, scope),
  time_converter_(time::Converter::instance())
{
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    Log::trace() << "counter::Writer::handle: time=" << sample->time << ", nr=" << sample->v.nr
                 << ", leader-count=" << sample->v.values[0];
    auto tp = time_converter_(sample->time);

    counters_.read(&sample->v);
    counter_writer_.write(counters_, tp);
    return false;
}
}
}
}
