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

#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/perf/io_reader.hpp>
#include <lo2s/perf/multi_reader.hpp>

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/trace/trace.hpp>

#include <map>
#include <memory>

namespace lo2s
{
namespace monitor
{

template <class Writer>
class IoMonitor : public PollMonitor
{
public:
    IoMonitor(trace::Trace& trace)
    : monitor::PollMonitor(trace, "", config().perf_read_interval), multi_reader_(trace)

    {
        for (auto fd : multi_reader_.get_fds())
        {
            add_fd(fd);
        }
    }

private:
    void monitor(int fd) override
    {
        if (fd == timer_pfd().fd)
        {
            return;
        }
        else
        {
            multi_reader_.read();
        }
    }

    void finalize_thread() override
    {
        multi_reader_.finalize();
    }

    std::string group() const override
    {
        return "lo2s::IoMonitor";
    }

private:
    perf::MultiReader<Writer> multi_reader_;
};

} // namespace monitor
} // namespace lo2s
