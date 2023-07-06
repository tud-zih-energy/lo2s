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

#include <lo2s/monitor/nec_thread_monitor.hpp>
#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types.hpp>

#include <veosinfo/veosinfo.h>

#include <libved.h>

#include <filesystem>
namespace lo2s
{
namespace monitor
{
class NecMonitorMain : public ThreadedMonitor
{
public:
    NecMonitorMain(trace::Trace& trace, int device);

    void stop() override;

protected:
    std::string group() const override
    {
        return "nec::MonitorMain";
    }

    void run() override;
    void finalize_thread() override;

private:
    std::map<Thread, NecThreadMonitor> monitors_;
    trace::Trace& trace_;
    int device_;
    bool stopped_;
    struct ve_nodeinfo nodeinfo_;
};
} // namespace monitor
} // namespace lo2s
