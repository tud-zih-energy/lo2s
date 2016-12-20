/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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
#include "otf2_counters.hpp"

namespace lo2s
{

otf2_counters::otf2_counters(pid_t pid, pid_t tid, otf2_trace& trace,
                             otf2::definition::metric_class metric_class,
                             otf2::definition::location scope)
: writer_(trace.metric_writer(pid, tid)),
  metric_instance_(trace.metric_instance(metric_class, writer_.location(), scope)),
  counters_{ { { tid, PERF_COUNT_HW_INSTRUCTIONS },
               { tid, PERF_COUNT_HW_CPU_CYCLES },
               { tid, get_mem_event(MEM_LEVEL_L2), PERF_TYPE_RAW },
               { tid, get_mem_event(MEM_LEVEL_L3), PERF_TYPE_RAW },
               { tid, get_mem_event(MEM_LEVEL_RAM), PERF_TYPE_RAW } } },
  proc_stat_(boost::filesystem::path("/proc") / std::to_string(pid) / "task" / std::to_string(tid) /
             "stat")
{
    auto mc = metric_instance_.metric_class();
    assert(counters_.size() <= mc.size());
    // TODO fix this sad interface :[
    values_.resize(mc.size());
    for (std::size_t i = 0; i < mc.size(); i++)
    {
        values_[i].metric = mc[i];
    }
}

otf2::definition::metric_class otf2_counters::get_metric_class(otf2_trace& trace)
{
    auto c = trace.metric_class();
    c.add_member(trace.metric_member("instructions", "instructions",
                                     otf2::common::metric_mode::accumulated_start,
                                     otf2::common::type::Double, "#"));
    c.add_member(trace.metric_member("cycles", "CPU cycles",
                                     otf2::common::metric_mode::accumulated_start,
                                     otf2::common::type::Double, "#"));
    c.add_member(trace.metric_member("L2", "Level 2 cache accesses",
                                     otf2::common::metric_mode::accumulated_start,
                                     otf2::common::type::Double, "#"));
    c.add_member(trace.metric_member("L3", "Level 3 cache accesses",
                                     otf2::common::metric_mode::accumulated_start,
                                     otf2::common::type::Double, "#"));
    c.add_member(trace.metric_member("RAM", "Ram accesses",
                                     otf2::common::metric_mode::accumulated_start,
                                     otf2::common::type::Double, "#"));
    c.add_member(trace.metric_member("CPU", "cpu executing the task",
                                     otf2::common::metric_mode::absolute_last,
                                     otf2::common::type::int64, "cpuid"));
    c.add_member(trace.metric_member("time_enabled", "time event active",
                                     otf2::common::metric_mode::accumulated_start,
                                     otf2::common::type::uint64, "ns"));
    c.add_member(trace.metric_member("time_running", "time event on CPU",
                                     otf2::common::metric_mode::accumulated_start,
                                     otf2::common::type::uint64, "ns"));
    return c;
}

void otf2_counters::write()
{
    auto read_time = get_time();

    assert(counters_.size() <= values_.size());
    for (std::size_t i = 0; i < counters_.size(); i++)
    {
        values_[i].set(counters_[i].read());
    }
    // FIXME this shouldn't be hardcoded number :(
    values_[5].set(get_task_last_cpu_id(proc_stat_));
    values_[6].set(counters_[0].enabled());
    values_[7].set(counters_[0].running());

    writer_.write(otf2::event::metric(read_time, metric_instance_, values_));
}
}
