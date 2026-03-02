// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/tracepoint/writer.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types/cpu.hpp>

#include <map>
#include <memory>
#include <string>

namespace lo2s::monitor
{

class TracepointMonitor : public PollMonitor
{
public:
    TracepointMonitor(trace::Trace& trace, Cpu cpu);

private:
    void monitor(int fd) override;
    void initialize_thread() override;
    void finalize_thread() override;

    std::string group() const override
    {
        return "tracepoint::TracepointMonitor";
    }

    Cpu cpu_;
    std::map<int, std::unique_ptr<perf::tracepoint::Writer>> perf_writers_;
};
} // namespace lo2s::monitor
