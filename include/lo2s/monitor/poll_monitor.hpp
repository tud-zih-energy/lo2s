/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/pipe.hpp>
#include <lo2s/trace/fwd.hpp>

#include <chrono>
#include <vector>

extern "C"
{
#include <poll.h>
}

namespace lo2s
{
namespace monitor
{
class PollMonitor : public ThreadedMonitor
{
public:
    PollMonitor(trace::Trace& trace, const std::string& name,
                std::chrono::nanoseconds read_interval);

    void stop() override;

    ~PollMonitor();

protected:
    void run() override;
    void monitor();

    void add_fd(int fd);

    virtual void monitor([[maybe_unused]] int fd) {};

    struct pollfd& stop_pfd()
    {
        return pfds_[0];
    }

    struct pollfd& timer_pfd()
    {
        return pfds_[1];
    }

    Pipe stop_pipe_;

private:
    std::vector<pollfd> pfds_;
};
} // namespace monitor
} // namespace lo2s
