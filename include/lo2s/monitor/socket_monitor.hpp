// SPDX-FileCopyrightText: 2016-2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/gpu_monitor.hpp>
#include <lo2s/monitor/openmp_monitor.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/trace/fwd.hpp>

#include <map>
#include <string>

namespace lo2s::monitor
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
} // namespace lo2s::monitor
