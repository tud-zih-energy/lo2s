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

#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types.hpp>
#include <lo2s/util.hpp>

#include <otf2xx/otf2.hpp>

#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <vector>

namespace lo2s
{
namespace perf
{
namespace bio
{

enum class BioEventType
{
    INSERT,
    ISSUE,
    COMPLETE
};

struct BioEvent
{
    BioEvent(dev_t device, uint64_t sector, uint64_t time, BioEventType type,
             otf2::common::io_operation_mode_type mode, uint32_t nr_sector)
    : device(device), sector(sector), time(time), type(type), mode(mode), nr_sector(nr_sector)
    {
    }
    dev_t device;
    uint64_t sector;
    uint64_t time;
    BioEventType type;
    otf2::common::io_operation_mode_type mode;
    uint32_t nr_sector;
};

class BioComperator
{
public:
    bool operator()(const BioEvent& t1, const BioEvent& t2)
    {
        return t1.time > t2.time;
    }
};

class Writer
{
public:
    Writer(trace::Trace& trace, BlockDevice& device)
    : trace_(trace), time_converter_(time::Converter::instance()), device_(device)
    {
    }

    void submit_events(const std::vector<BioEvent>& new_events_)
    {
        std::lock_guard<std::mutex> guard(lock_);
        for (auto& event : new_events_)
        {
            events_.emplace(event);
        }
    }

    void write_events()
    {
        if (events_.empty())
        {
            return;
        }
        otf2::writer::local& writer = trace_.bio_writer(device_);
        otf2::definition::io_handle& handle = trace_.block_io_handle(device_);
        while (!events_.empty())
        {
            const BioEvent& event = events_.top();
            if (event.type == BioEventType::INSERT)
            {
                writer << otf2::event::io_operation_begin(
                    time_converter_(event.time), handle, event.mode,
                    otf2::common::io_operation_flag_type::non_blocking, event.nr_sector,
                    event.sector);
            }
            else if (event.type == BioEventType::ISSUE)
            {
                writer << otf2::event::io_operation_issued(time_converter_(event.time), handle,
                                                           event.sector);
            }
            else
            {
                writer << otf2::event::io_operation_complete(time_converter_(event.time), handle,
                                                             event.nr_sector, event.sector);
            }
            events_.pop();
        }
    }

private:
    trace::Trace& trace_;
    const time::Converter& time_converter_;
    BlockDevice device_;
    std::priority_queue<BioEvent, std::vector<BioEvent>, BioComperator> events_;
    std::mutex lock_;
};

} // namespace bio
} // namespace perf
} // namespace lo2s
