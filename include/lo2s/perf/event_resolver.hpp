// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/perf/event_attr.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lo2s::perf
{

class EventResolver
{
public:
    EventAttr get_event_by_name(const std::string& name);

    bool has_event(const std::string& name);

    std::vector<EventAttr> get_predefined_events();
    std::vector<SysfsEventAttr> get_pmu_events();

    EventAttr get_metric_leader(const std::string& metric_leader);

    std::vector<std::string> get_tracepoint_event_names();

    static EventResolver& instance()
    {
        static EventResolver e;
        return e;
    }

    EventResolver();
    EventResolver(const EventResolver&) = delete;
    EventResolver& operator=(const EventResolver&) = delete;
    EventResolver(const EventResolver&&) = delete;
    EventResolver& operator=(const EventResolver&&) = delete;

    ~EventResolver() = default;

private:
    EventAttr cache_event(const std::string& name);
    EventAttr fallback_metric_leader_event();

    std::unordered_map<std::string, std::optional<EventAttr>> event_map_;
};

} // namespace lo2s::perf
