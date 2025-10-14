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

#pragma once

#include "lo2s/topology.hpp"
#include <cstdint>
#include <lo2s/perf/event_attr.hpp>

#include <cstring>
#include <string>
#include <vector>

#include <fmt/core.h>

extern "C"
{
#include <linux/perf_event.h>
#include <pmu-events/pmu-events.h>
}

namespace lo2s
{
namespace perf
{

class PMUEvents
{
public:
    static const PMUEvents& instance()
    {
        static PMUEvents pmu_events;
        return pmu_events;
    }

    EventAttr read_event(const std::string& ev_desc) const
    {

        struct pmu_event pmu_ev;

        std::set<std::string> visited_cpuids;
        for (auto& cpu : Topology::instance().cpus())
        {
            struct perf_cpu pcpu;
            pcpu.cpu = cpu.as_int();
            if (get_event_by_name(map_for_cpu(pcpu), ev_desc.c_str(), &pmu_ev) == -1)
            {
                continue;
            }
            struct perf_event_attr attr;

            memset(&attr, 0, sizeof(attr));

            if (gen_attr_for_event(&pmu_ev, pcpu, &attr) == -1)
            {
                throw EventAttr::InvalidEvent(
                    fmt::format("Can not get perf_event_attr for pmu-events event {}!", ev_desc));
            }

            EventAttr ev =
                PredefinedEventAttr(ev_desc, (perf_type_id)attr.type, attr.config, attr.config1);

            return ev;
        }
        throw EventAttr::InvalidEvent(fmt::format("No pmu-events event named {}!", ev_desc));
    }

    std::vector<EventAttr> get_pmu_events() const
    {
        std::vector<EventAttr> events;

        std::set<std::string> visited_cpuids;
        for (auto& cpu : Topology::instance().cpus())
        {
            struct perf_cpu pcpu;

            pcpu.cpu = cpu.as_int();
            const struct pmu_events_map* maps = map_for_cpu(pcpu);

            if (visited_cpuids.find(maps->cpuid) != visited_cpuids.end())
            {
                continue;
            }

            visited_cpuids.emplace(maps->cpuid);

            for (uint32_t i = 0; i < maps->event_table.num_pmus; i++)
            {
                struct pmu_table_entry entry = maps->event_table.pmus[i];

                for (uint32_t x = 0; x < entry.num_entries; x++)
                {
                    struct pmu_event ev;
                    decompress_event(entry.entries[x].offset, &ev);

                    try
                    {
                        auto event = read_event(std::string(ev.name));
                        events.emplace_back(event);
                    }
                    catch (EventAttr::InvalidEvent& e)
                    {
                    }
                }
            }
        }

        return events;
    }
};

} // namespace perf
} // namespace lo2s
