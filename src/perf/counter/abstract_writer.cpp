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
#include <lo2s/perf/counter/abstract_writer.hpp>
#include <lo2s/perf/event_collection.hpp>
#include <lo2s/time/time.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
AbstractWriter::AbstractWriter(pid_t tid, int cpuid, otf2::writer::local& writer, otf2::definition::metric_instance metric_instance,
               bool enable_on_exec)
: Reader(tid, cpuid, requested_events(), enable_on_exec),
  time_converter_(time::Converter::instance()),
  writer_(writer),
  metric_instance_(metric_instance)
{
    auto mc = metric_instance_.metric_class();

    values_.resize(mc.size());
    for (std::size_t i = 0; i < mc.size(); i++)
    {
        values_[i].metric = mc[i];
    }
}

bool AbstractWriter::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);

    counters_.read(&sample->v);

    assert(counters_.size() <= values_.size());

    for (std::size_t i = 0; i < counters_.size(); i++)
    {
        values_[i].set(counters_[i]);
    }

    auto index = counters_.size();
    values_[index++].set(counters_.enabled());
    values_[index].set(counters_.running());
    
    handle_custom_events();

    // TODO optimize! (avoid copy, avoid shared pointers...)
    writer_.write(otf2::event::metric(tp, metric_instance_, values_));
    return false;
}
}
}
}
