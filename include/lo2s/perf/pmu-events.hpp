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

#include <lo2s/perf/event_attr.hpp>
#include <lo2s/topology.hpp>

#include <regex>
#include <string>
#include <vector>

#include <cstdint>
#include <cstring>

#include <fmt/core.h>
#include <fmt/format.h>

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

    EventAttr read_event(std::string ev_desc) const
    {

        struct pmus pmus;

        get_pmus(&pmus);

        std::regex inst_and_ev("(.*)::(.*)");
        std::smatch match;

        if (std::regex_match(ev_desc, match, inst_and_ev))
        {
            for (size_t cur_class_id = 0; cur_class_id < pmus.num_classes; cur_class_id++)
            {
                struct pmu_class* cur_class = &pmus.classes[cur_class_id];
                for (int cur_instance_id = 0; cur_instance_id < cur_class->num_instances;
                     cur_instance_id++)
                {
                    struct pmu_instance* cur_instance = &cur_class->instances[cur_instance_id];
                    if (match[1] == cur_instance->name)
                    {
                        for (uint32_t cur_event_id = 0; cur_event_id < cur_instance->num_entries;
                             cur_event_id++)
                        {

                            struct pmu_event ev;
                            decompress_event(cur_instance->entries[cur_event_id].offset, &ev);

                            if (match[2] == ev.name)
                            {
                                auto event = read_event(cur_instance, &ev);
                                free_pmus(&pmus);
                                return event;
                            }
                        }
                    }
                }
            }
        }

        free_pmus(&pmus);

        throw EventAttr::InvalidEvent(fmt::format("No event named {}!", ev_desc));
    }

    std::vector<EventAttr> get_pmu_events() const
    {
        std::vector<EventAttr> events;

        struct pmus pmus;

        get_pmus(&pmus);
        for (size_t cur_class_id = 0; cur_class_id < pmus.num_classes; cur_class_id++)
        {
            struct pmu_class* cur_class = &pmus.classes[cur_class_id];
            for (int cur_instance_id = 0; cur_instance_id < cur_class->num_instances;
                 cur_instance_id++)
            {
                struct pmu_instance* cur_instance = &cur_class->instances[cur_instance_id];
                for (uint32_t cur_event_id = 0; cur_event_id < cur_instance->num_entries;
                     cur_event_id++)
                {
                    struct pmu_event ev;
                    decompress_event(cur_instance->entries[cur_event_id].offset, &ev);

                    try
                    {
                        auto event = read_event(cur_instance, &ev);
                        events.emplace_back(event);
                    }
                    catch (EventAttr::InvalidEvent& e)
                    {
                        Log::trace()
                            << "Could not read perf PMU event " << ev.name << ": " << e.what();
                    }
                }
            }
        }

        return events;
    }

private:
    std::set<Cpu> range_list_to_set(const struct range_list* range_list) const
    {
        std::set<Cpu> res;
        for (size_t range_id = 0; range_id < range_list->len; range_id++)
        {
            struct range range = range_list->ranges[range_id];

            for (uint64_t id = range.start; id <= range.end; id++)
            {
                res.emplace(Cpu(id));
            }
        }
        return res;
    }

    EventAttr read_event(const struct pmu_instance* inst, const struct pmu_event* pmu_ev) const
    {

        std::string ev_name = fmt::format("{}::{}", inst->name, pmu_ev->name);

        struct perf_event_attr attr;

        memset(&attr, 0, sizeof(attr));

        if (gen_attr_for_event(inst, pmu_ev, &attr) == -1)
        {
            throw EventAttr::InvalidEvent(
                fmt::format("Can not get perf_event_attr for pmu-events event {}!", ev_name));
        }

        return PredefinedEventAttr(ev_name, (perf_type_id)attr.type, attr.config, attr.config1,
                                   range_list_to_set(&inst->cpus));
    }
};

} // namespace perf
} // namespace lo2s
