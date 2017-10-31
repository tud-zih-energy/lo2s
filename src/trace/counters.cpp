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

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/platform.hpp>

namespace lo2s
{
namespace trace
{
// TODO This is an interdependent ball of ... please clean this up
Counters::Counters(pid_t pid, pid_t tid, Trace& trace, otf2::definition::metric_class metric_class,
                   otf2::definition::location scope)
: writer_(trace.metric_writer(pid, tid)),
  metric_instance_(trace.metric_instance(metric_class, writer_.location(), scope)),
  proc_stat_(boost::filesystem::path("/proc") / std::to_string(pid) / "task" / std::to_string(tid) /
             "stat")
{
    auto mc = metric_instance_.metric_class();

    values_.resize(mc.size());
    for (std::size_t i = 0; i < mc.size(); i++)
    {
        values_[i].metric = mc[i];
    }
}

otf2::definition::metric_class Counters::get_metric_class(Trace& trace_)
{
    auto c = trace_.metric_class();
    const auto& user_events = lo2s::config().perf_events;

    for (const auto& ev : user_events)
    {
        if (perf::EventProvider::has_event(ev))
        {
            c.add_member(trace_.metric_member(ev, ev, otf2::common::metric_mode::accumulated_start,
                                              otf2::common::type::Double, "#"));
        }
    }

    if (user_events.size() == 0)
    {
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

std::vector<lo2s::perf::CounterDescription> Counters::collect_counters()
{
    const auto& mem_events = platform::get_mem_events();
    const auto& user_events = lo2s::config().perf_events;

    std::vector<perf::CounterDescription> used_counters;

    used_counters.reserve(mem_events.size() + user_events.size());
    for (const auto& ev : user_events)
    {
        try
        {
            const auto event_desc = perf::EventProvider::get_event_by_name(ev);
            used_counters.emplace_back(event_desc);
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "'" << ev
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }

    if (user_events.size() == 0)
    {
        for (const auto& description : mem_events)
        {
            used_counters.emplace_back(description);
        }

        used_counters.emplace_back(perf::EventProvider::get_event_by_name("instructions"));
        used_counters.emplace_back(perf::EventProvider::get_event_by_name("cpu-cycles"));
    }

    return used_counters;
}

void Counters::write(const metric::PerfCounterGroup& counters, otf2::chrono::time_point read_time)
{
    assert(counters.size() <= values_.size());

    for (std::size_t i = 0; i < counters.size(); i++)
    {
        values_[i].set(counters[i]);
    }
    auto index = counters.size();
    values_[index++].set(get_task_last_cpu_id(proc_stat_));
    values_[index++].set(counters.enabled());
    values_[index].set(counters.running());

    // TODO optimize! (avoid copy, avoid shared pointers...)
    writer_.write(otf2::event::metric(read_time, metric_instance_, values_));
}
}
}
