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

#include <lo2s/perf/bio/event_cacher.hpp>
#include <lo2s/perf/bio/writer.hpp>

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/trace/trace.hpp>

#include <map>
#include <memory>

namespace lo2s
{
namespace monitor
{

class BioMonitor : public PollMonitor
{
public:
    BioMonitor(trace::Trace& trace, Cpu cpu,
               std::map<dev_t, std::unique_ptr<perf::bio::Writer>>& writers_);

private:
    void monitor(int fd) override;
    void initialize_thread() override;
    void finalize_thread() override;

    std::string group() const override
    {
        return "lo2s::BioMonitor";
    }

private:
    Cpu cpu_;
    perf::bio::EventCacher bio_insert_cacher_;
    perf::bio::EventCacher bio_issue_cacher_;
    perf::bio::EventCacher bio_complete_cacher_;
};
} // namespace monitor
} // namespace lo2s
