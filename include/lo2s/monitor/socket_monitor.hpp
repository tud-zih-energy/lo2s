/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016-2018, Technische Universitaet Dresden, Germany
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

#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/resolvers.hpp>

#include <lo2s/monitor/gpu_monitor.hpp>
#include <lo2s/monitor/openmp_monitor.hpp>

extern "C"
{
#include <sched.h>
#include <unistd.h>
}

namespace lo2s
{
namespace monitor
{

class SocketMonitor : public PollMonitor
{
public:
    SocketMonitor(trace::Trace& trace);

    void finalize_thread() override;
    void monitor(int fd) override;

    std::string group() const override
    {
        return "lo2s::SocketMonitor";
    }

    void emplace_resolvers(Resolvers& resolvers)
    {
        for (auto& monitor : gpu_monitors_)
        {
            monitor.second.emplace_resolvers(resolvers);
        }
    }

private:
    trace::Trace& trace_;
    std::map<int, GPUMonitor> gpu_monitors_;
    std::map<int, OpenMPMonitor> openmp_monitors_;

    int socket = -1;
};
} // namespace monitor
} // namespace lo2s
