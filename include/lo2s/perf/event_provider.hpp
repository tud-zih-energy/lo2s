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

#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <lo2s/perf/tracepoint/event.hpp>

namespace lo2s
{
namespace perf
{

class EventProvider
{
public:
    EventProvider();
    EventProvider(const EventProvider&) = delete;
    void operator=(const EventProvider&) = delete;

    static const EventProvider& instance()
    {
        return instance_mutable();
    }

    static Event get_event_by_name(const std::string& name);

    static bool has_event(const std::string& name);

    static std::vector<Event> get_predefined_events();
    static std::vector<SysfsEvent> get_pmu_events();

    static Event fallback_metric_leader_event();

    static Event create_time_event(std::uint64_t local_time, bool enable_on_exec = false);
    static Event create_event(const std::string& name, perf_type_id type, std::uint64_t config,
                              std::uint64_t config1 = 0);
    static SysfsEvent create_sampling_event(bool enable_on_exec);
    static SysfsEvent create_sysfs_event(const std::string& name, bool use_config = true);
    static tracepoint::TracepointEvent create_tracepoint_event(const std::string& name,
                                                               bool use_config = true,
                                                               bool enable_on_exec = false);

    class InvalidEvent : public std::runtime_error
    {
    public:
        InvalidEvent(const std::string& event_description)
        : std::runtime_error(std::string{ "Invalid event: " } + event_description)
        {
        }
    };

private:
    static EventProvider& instance_mutable()
    {
        static EventProvider e;
        return e;
    }

    static void apply_config_attrs(Event& event);
    static void apply_default_attrs(Event& event);

    Event cache_event(const std::string& name);

    std::unordered_map<std::string, Event> event_map_;
};

} // namespace perf
} // namespace lo2s
