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
#include <lo2s/perf/multi_reader.hpp>

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
    BioMonitor(trace::Trace& trace);

private:
    void monitor(int fd) override;
    void finalize_thread() override;

    std::string group() const override
    {
        return "lo2s::BioMonitor";
    }

private:
    perf::MultiReader<perf::bio::Reader, perf::bio::Writer> multi_reader_;
};

} // namespace monitor
} // namespace lo2s
