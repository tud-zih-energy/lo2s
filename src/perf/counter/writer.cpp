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
Writer::Writer(pid_t pid, pid_t tid, int cpuid, otf2::writer::local& writer, trace::Trace& trace, otf2::definition::location scope,
               bool enable_on_exec)
: Reader(tid, cpuid, requested_events(), enable_on_exec),
  time_converter_(time::Converter::instance()),
  writer_(writer),
  metric_instance_(trace.metric_instance(get_metric_class(trace), writer_.location(),
              scope)),
  proc_stat_(boost::filesystem::path("/proc") / std::to_string(pid) / "task" / std::to_string(tid) /
             "stat"),
  cpuid_(cpuid)
{
    auto mc = metric_instance_.metric_class();

    values_.resize(mc.size());
    for (std::size_t i = 0; i < mc.size(); i++)
    {
        values_[i].metric = mc[i];
    }
}

otf2::definition::metric_class Writer::get_metric_class(trace::Trace& trace_)
{
    auto c = trace_.metric_class();

    const perf::EventCollection& event_collection = perf::requested_events();

    c.add_member(trace_.metric_member(event_collection.leader.name, event_collection.leader.name,
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::Double, "#"));

    for (const auto& ev : event_collection.events)
    {
        c.add_member(trace_.metric_member(ev.name, ev.name,
                                          otf2::common::metric_mode::accumulated_start,
                                          otf2::common::type::Double, "#"));
    }

    c.add_member(trace_.metric_member("CPU", "CPU executing the task",
                                      otf2::common::metric_mode::absolute_last,
                                      otf2::common::type::int64, "cpuid"));
    c.add_member(trace_.metric_member("time_enabled", "time event active",
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::uint64, "ns"));
    c.add_member(trace_.metric_member("time_running", "time event on CPU",
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::uint64, "ns"));
    return c;
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);

    counters_.read(&sample->v);

    assert(counters_.size() <= values_.size());

    for (std::size_t i = 0; i < counters_.size(); i++)
    {
        values_[i].set(counters_[i]);
    }

    auto index = counters_.size();
    if(cpuid_ == -1)
    {
        values_[index++].set(get_task_last_cpu_id(proc_stat_));
    }
    else
    {
        values_[index++].set(cpuid_);
    }
    values_[index++].set(counters_.enabled());
    values_[index].set(counters_.running());

    // TODO optimize! (avoid copy, avoid shared pointers...)
    writer_.write(otf2::event::metric(tp, metric_instance_, values_));
    return false;
}
}
}
}
