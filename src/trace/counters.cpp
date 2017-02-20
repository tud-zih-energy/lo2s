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

#include <lo2s/trace/counters.hpp>

#include <lo2s/platform.hpp>

namespace lo2s
{
namespace trace
{

counters::counters(pid_t pid, pid_t tid, trace& trace_, otf2::definition::metric_class metric_class,
                   otf2::definition::location scope)
: writer_(trace_.metric_writer(pid, tid)),
  metric_instance_(trace_.metric_instance(metric_class, writer_.location(), scope)),
  proc_stat_(boost::filesystem::path("/proc") / std::to_string(pid) / "task" / std::to_string(tid) /
             "stat")
{
    auto mc = metric_instance_.metric_class();
    const auto& mem_events = platform::get_mem_events();
    counters_.reserve(mem_events.size());
    for (const auto& description : mem_events)
    {
        counters_.emplace_back(tid, description.type, description.config, description.config1);
    }
    counters_.emplace_back(tid, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
    counters_.emplace_back(tid, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);

    assert(counters_.size() <= mc.size());

    values_.resize(mc.size());
    for (std::size_t i = 0; i < mc.size(); i++)
    {
        values_[i].metric = mc[i];
    }
}

otf2::definition::metric_class counters::get_metric_class(trace& trace_)
{
    auto c = trace_.metric_class();

    for (const auto& description : platform::get_mem_events())
    {
        c.add_member(trace_.metric_member(description.name, description.name,
                                          otf2::common::metric_mode::accumulated_start,
                                          otf2::common::type::Double, "#"));
    }

    c.add_member(trace_.metric_member("instructions", "instructions",
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::Double, "#"));
    c.add_member(trace_.metric_member("cycles", "CPU cycles",
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::Double, "#"));

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

void counters::write()
{
    auto read_time = time::now();

    assert(counters_.size() <= values_.size());
    for (std::size_t i = 0; i < counters_.size(); i++)
    {
        values_[i].set(counters_[i].read());
    }
    auto index = counters_.size();
    values_[index++].set(get_task_last_cpu_id(proc_stat_));
    values_[index++].set(counters_[0].enabled());
    values_[index].set(counters_[0].running());

    // TODO optimize! (avoid copy, avoid shared pointers...)
    writer_.write(otf2::event::metric(read_time, metric_instance_, values_));
}
}
}
