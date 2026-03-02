// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/multi_reader.hpp>
#include <lo2s/trace/fwd.hpp>

#include <string>

namespace lo2s::monitor
{

template <class Writer>
class IoMonitor : public PollMonitor
{
public:
    IoMonitor(trace::Trace& trace) : monitor::PollMonitor(trace, "IoMonitor"), multi_reader_(trace)

    {
        for (auto fd : multi_reader_.get_fds())
        {
            add_fd(fd);
        }
    }

private:
    void monitor(int fd [[maybe_unused]]) override
    {
        multi_reader_.read();
    }

    void finalize_thread() override
    {
        multi_reader_.finalize();
    }

    std::string group() const override
    {
        return "lo2s::IoMonitor";
    }

    perf::MultiReader<Writer> multi_reader_;
};

} // namespace lo2s::monitor
