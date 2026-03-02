// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/trace/fwd.hpp>

#include <string>
#include <vector>

extern "C"
{
#include <sys/poll.h>
}

namespace lo2s::monitor
{
class PollMonitor : public ThreadedMonitor
{
public:
    PollMonitor(trace::Trace& trace, const std::string& name);

    PollMonitor(const PollMonitor&) = delete;
    PollMonitor& operator=(const PollMonitor&) = delete;
    PollMonitor(PollMonitor&&) = delete;
    PollMonitor& operator=(PollMonitor&&) = delete;

    void stop() override;

    ~PollMonitor() override = default;

protected:
    void run() override;
    void monitor();

    void add_fd(int fd);

    virtual void monitor([[maybe_unused]] int fd) {};

    struct pollfd& stop_pfd()
    {
        return pfds_[0];
    }

private:
    std::vector<pollfd> pfds_;
};
} // namespace lo2s::monitor
