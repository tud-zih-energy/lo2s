// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types/nec_device.hpp>
#include <lo2s/types/thread.hpp>

#include <atomic>
#include <optional>
#include <string>

extern "C"
{
#include <libved.h>
#include <veosinfo/veosinfo.h>
}

namespace lo2s::nec
{
class NecMonitorMain : public monitor::ThreadedMonitor
{
public:
    NecMonitorMain(trace::Trace& trace, NecDevice device);

    void stop() override;

protected:
    std::string group() const override
    {
        return "nec::MonitorMain";
    }

    void run() override;
    void finalize_thread() override;

private:
    std::optional<NecDevice> get_device_of(Thread thread);
    std::vector<Thread> get_tasks_of(NecDevice device);
    std::map<Thread, NecThreadMonitor> monitors_;
    trace::Trace& trace_;
    NecDevice device_;
    std::atomic<bool> stopped_;
    ve_nodeinfo nodeinfo_;
};
} // namespace lo2s::nec
