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

#include <lo2s/perf/event_description.hpp>

#include <cstring>
#include <string>
#include <vector>

#include <fmt/core.h>

extern "C"
{
#include <perfmon/pfmlib.h>
// Great APIs are known for creating tons of redefinition errors if they are not included in the
// right order
#include <linux/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>
}

namespace lo2s
{
namespace perf
{

class PFM4
{
public:
    static const PFM4& instance()
    {
        static PFM4 pfm4;
        return pfm4;
    }
    ~PFM4()
    {
        pfm_terminate();
    }

    const EventDescription pfm4_read_event(const std::string& ev_desc) const
    {
        pfm_perf_encode_arg_t arg;
        struct perf_event_attr attr;

        memset(&arg, 0, sizeof(arg));
        memset(&attr, 0, sizeof(attr));
        arg.attr = &attr;

        int ret = pfm_get_os_event_encoding(ev_desc.c_str(), PFM_PLM3, PFM_OS_PERF_EVENT, &arg);

        if (ret != PFM_SUCCESS)
        {
            return EventDescription();
        }

        EventDescription ev =
            EventDescription(ev_desc, (perf_type_id)attr.type, attr.config, attr.config1);

        return ev;
    }

    void print_pfm4_events() const
    {
        pfm_pmu_info_t pinfo;
        pfm_event_info_t info;

        for (int pmu_id = PFM_PMU_NONE; pmu_id < PFM_PMU_MAX; pmu_id++)
        {
            memset(&pinfo, 0, sizeof(pinfo));

            if (pfm_get_pmu_info((pfm_pmu_t)pmu_id, &pinfo) != PFM_SUCCESS)
            {
                continue;
            }

            for (int event_id = pinfo.first_event; event_id != -1;
                 event_id = pfm_get_event_next(event_id))
            {
                memset(&info, 0, sizeof(info));
                if (pfm_get_event_info(event_id, PFM_OS_PERF_EVENT, &info) != PFM_SUCCESS)
                {
                    continue;
                }

                std::string full_event_name = fmt::format("{}::{}", pinfo.name, info.name);
                // Some events are confusingly more like event groups, for which we have to retrieve
                // the subevents
                if (info.nattrs != 0)
                {
                    std::vector<std::string> uevents;
                    for (int attr_id = 0; attr_id < info.nattrs; attr_id++)
                    {
                        pfm_event_attr_info_t attr_info;
                        memset(&attr_info, 0, sizeof(attr_info));
                        pfm_get_event_attr_info(info.idx, attr_id, PFM_OS_PERF_EVENT, &attr_info);

                        if (attr_info.type != PFM_ATTR_UMASK)
                        {
                            continue;
                        }
                        else
                        {
                            EventDescription uevent = pfm4_read_event(
                                fmt::format("{}:{}", full_event_name, attr_info.name));
                            if (uevent.is_valid())
                            {
                                uevents.emplace_back(std::string(attr_info.name));
                            }
                        }
                    }

                    if (!uevents.empty())
                    {
                        if (uevents.size() == 1)
                        {
                            std::cout << " " << full_event_name << ":" << uevents[0] << std::endl;
                        }
                        else
                        {
                            std::cout << " " << full_event_name << std::endl;
                            for (auto& event : uevents)
                            {
                                std::cout << " \t- " << event << std::endl;
                            }
                        }
                    }
                    else
                    {
                        EventDescription event = pfm4_read_event(std::string(full_event_name));

                        if (event.is_valid())
                        {
                            std::cout << " " << full_event_name << std::endl;
                        }
                    }
                }
                else
                {
                    EventDescription event = pfm4_read_event(std::string(full_event_name));

                    if (event.is_valid())
                    {
                        std::cout << " " << full_event_name << std::endl;
                    }
                }
            }
        }
    }

private:
    PFM4()
    {
        pfm_initialize();
    }
};

} // namespace perf
} // namespace lo2s
