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

#include <lo2s/perf/bio/reader.hpp>
#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/perf/time/converter.hpp>

#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <unordered_map>
#include <vector>

namespace lo2s
{
namespace perf
{
namespace bio
{
// Note, this cannot be protected for CRTP reasons...

class EventCacher : public Reader<EventCacher>
{
public:
    struct __attribute((__packed__)) RecordBlock
    {
        uint16_t common_type;         // 2
        uint8_t common_flag;          // 3
        uint8_t common_preempt_count; // 4
        int32_t common_pid;           // 8

        uint32_t dev; // 12
        char padding[4];
        uint64_t sector; // 24

        uint32_t nr_sector; // 28
        int32_t error_or_bytes;

        char rwbs[8]; // 40
    };

    EventCacher(Cpu cpu, std::map<dev_t, std::unique_ptr<Writer>>& writers, BioEventType type);

    EventCacher(const EventCacher& other) = delete;

    EventCacher(EventCacher&& other) = default;

public:
    using Reader<EventCacher>::handle;

    bool handle(const Reader::RecordSampleType* sample);

    void end()
    {
        for (auto& events : events_)
        {
            writers_[events.first]->submit_events(events.second);
        }
    }

private:
    std::map<dev_t, std::unique_ptr<Writer>>& writers_;
    std::unordered_map<dev_t, std::vector<BioEvent>> events_;
};

} // namespace bio
} // namespace perf
} // namespace lo2s
