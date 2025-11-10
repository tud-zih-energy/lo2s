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

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/util.hpp>

#include <cmath>

extern "C"
{
#include <sys/eventfd.h>
}

namespace lo2s
{
namespace monitor
{
PollMonitor::PollMonitor(trace::Trace& trace, const std::string& name,
                         std::chrono::nanoseconds read_interval)
: ThreadedMonitor(trace, name)
{
    pfds_.resize(2);
    stop_pfd().fd = eventfd(0, 0);
    stop_pfd().events = POLLIN;
    stop_pfd().revents = 0;

    // Create and initialize timer_fd
    struct itimerspec tspec;
    memset(&tspec, 0, sizeof(struct itimerspec));

    // Set initial expiration to lowest possible value, this together with TFD_TIMER_ABSTIME should
    // synchronize our timers
    if (read_interval.count() != 0)
    {

        timer_pfd().fd = timerfd_from_ns(read_interval);
        timer_pfd().events = POLLIN;
        timer_pfd().revents = 0;
    }
    else
    {
        // fd = -1 is just going to be ignored
        timer_pfd().fd = -1;
        timer_pfd().events = 0;
        timer_pfd().revents = 0;
    }
}

void PollMonitor::add_fd(int fd, int events)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
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
    do
    {
        auto ret = ::poll(pfds_.data(), pfds_.size(), -1);
        num_wakeups_++;

        if (ret == 0)
        {
            throw std::runtime_error("Received poll timeout despite requesting no timeout.");
        }
        else if (ret < 0)
        {
            Log::error() << "poll failed";
            throw_errno();
        }
        Log::trace() << "PollMonitor poll returned " << ret;

        monitor();

        // Flush timer
        if (timer_pfd().revents & POLLIN)
        {
            [[maybe_unused]] uint64_t expirations;
            if (read(timer_pfd().fd, &expirations, sizeof(expirations)) == -1)
            {
                Log::error() << "Flushing timer fd failed";
                throw_errno();
            }
        }
        if (stop_pfd().revents & POLLIN)
        {
            Log::debug() << "Requested stop of PollMonitor";
            stop_requested = true;
        }
    } while (!stop_requested);
}

PollMonitor::~PollMonitor()
{
    if (timer_pfd().fd != -1)
    {
        close(timer_pfd().fd);
    }
}

} // namespace monitor
} // namespace lo2s
