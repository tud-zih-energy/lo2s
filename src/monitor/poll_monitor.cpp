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

#include <lo2s/monitor/poll_monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/trace/trace.hpp>

#include <stdexcept>
#include <string>

#include <cstdint>

extern "C"
{
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <unistd.h>
}

namespace lo2s::monitor
{
PollMonitor::PollMonitor(trace::Trace& trace, const std::string& name)
: ThreadedMonitor(trace, name)
{
    pfds_.resize(1);
    stop_pfd().fd = eventfd(0, 0);
    stop_pfd().events = POLLIN;
    stop_pfd().revents = 0;
}

void PollMonitor::add_fd(int fd)
{
    struct pollfd const pfd{ fd, POLLIN, 0 };
    pfds_.push_back(pfd);
}

void PollMonitor::stop()
{
    if (!thread_.joinable())
    {
        Log::warn() << "Cannot stop/join PollMonitor thread not running.";
        return;
    }

    uint64_t stop_val = 1;
    write(stop_pfd().fd, &stop_val, sizeof(stop_val));
    thread_.join();
}

void PollMonitor::monitor()
{
    for (const auto& pfd : pfds_)
    {
        if (pfd.revents & POLLIN)
        {
            monitor(pfd.fd);
        }
    }
}

void PollMonitor::run()
{
    bool stop_requested = false;
    while (!stop_requested)
    {
        auto ret = ::poll(pfds_.data(), pfds_.size(), -1);
        num_wakeups_++;

        if (ret == 0)
        {
            throw std::runtime_error("Received poll timeout despite requesting no timeout.");
        }

        if (ret < 0)
        {
            Log::error() << "poll failed";
            throw_errno();
        }

        Log::trace() << "PollMonitor poll returned " << ret;

        bool panic = false;
        for (const auto& pfd : pfds_)
        {
            if (pfd.revents != 0 && pfd.revents != POLLIN)
            {
                Log::warn() << "Poll on raw event fds got unexpected event flags: " << pfd.revents
                            << ". Stopping raw event polling.";
                panic = true;
            }
        }
        if (panic)
        {
            break;
        }

        monitor();

        if (stop_pfd().revents & POLLIN)
        {
            Log::debug() << "Requested stop of PollMonitor";
            stop_requested = true;
        }
    }
}
} // namespace lo2s::monitor
