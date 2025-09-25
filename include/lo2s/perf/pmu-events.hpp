/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2025,
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

#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/pmu.hpp>

#include <optional>
#include <string>
#include <vector>

#include <fmt/core.h>

#include <nitro/lang/string.hpp>

extern "C"
{
#include <linux/perf_event.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <pmu-events/pmu-events.h>
}

namespace lo2s
{
namespace perf
{

class PMUEventAttr : public EventAttr
{
public:
    PMUEventAttr(struct pmu_event ev) : EventAttr(ev.event, static_cast<perf_type_id>(0), 0)
    {
        name_ = ev.name;
        PMU pmu("cpu");

        struct perf_event_attr pmu_attr = pmu.ev_from_string(ev.event);
        attr_.type = pmu.type();
        attr_.config = pmu_attr.config;
        attr_.config1 = pmu_attr.config1;

        Log::debug() << std::hex << std::showbase << "parsed event description: " << pmu.name()
                     << "/" << name_ << "/type=" << attr_.type << ",config=" << attr_.config
                     << ",config1=" << attr_.config1 << std::dec << std::noshowbase << "/";

        if (ev.unit != nullptr)
        {
            unit(ev.unit);
        }

        get_cpu_set_for(*this);

        if (!event_is_openable())
        {
            throw EventAttr::InvalidEvent(
                "Event can not be opened in process- or system-monitoring-mode");
        }
    }
};

class PMUEvents
{
public:
    static const PMUEvents& instance()
    {
        static PMUEvents pmu_events;
        return pmu_events;
    }

    ~PMUEvents()
    {
    }

    EventAttr read_event(const std::string& ev_desc) const
    {
        auto ev = get_event_by_name(ev_desc);

        if (!ev.has_value())
        {
            throw EventAttr::InvalidEvent("Not a pmu-events event: " + ev_desc);
        }
        return PMUEventAttr(ev.value());
    }

    std::vector<EventAttr> get_events() const
    {
        std::vector<EventAttr> events;
        // TODO realize _real_ multi-cpu handling
        const struct pmu_events_map* map = map_for_cpu(perf_cpu{ 0 });

        if (map == nullptr)
        {
            return events;
        }

        for (uint32_t i = 0; i < map->event_table.num_pmus; i++)
        {
            struct pmu_table_entry entry = map->event_table.pmus[i];
            struct pmu_event pmu;
            decompress_event(entry.pmu_name.offset, &pmu);

            for (uint32_t x = 0; x < entry.num_entries; x++)
            {
                struct pmu_event ev;
                decompress_event(entry.entries[x].offset, &ev);

                try
                {
                    events.emplace_back(PMUEventAttr(ev));
                }
                catch (EventAttr::InvalidEvent& e)
                {
                }
            }
        }

        return events;
    }

private:
    std::optional<struct pmu_event> get_event_by_name(const std::string& ev_desc) const
    {
        const struct pmu_events_map* map = map_for_cpu(perf_cpu{ 0 });

        if (map == nullptr)
        {
            return std::nullopt;
        }

        for (uint32_t i = 0; i < map->event_table.num_pmus; i++)
        {
            struct pmu_table_entry entry = map->event_table.pmus[i];
            struct pmu_event pmu;
            decompress_event(entry.pmu_name.offset, &pmu);

            for (uint32_t x = 0; x < entry.num_entries; x++)
            {
                struct pmu_event ev;
                decompress_event(entry.entries[x].offset, &ev);

                if (ev_desc == ev.name)
                {
                    return ev;
                }
            }
        }

        return std::nullopt;
    }
};

} // namespace perf
} // namespace lo2s
