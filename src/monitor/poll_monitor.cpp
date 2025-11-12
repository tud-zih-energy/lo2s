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

#include "lo2s/helpers/fd.hpp"
#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/util.hpp>

#include <cmath>
#include <memory>

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
: ThreadedMonitor(trace, name), stop_fd_(EventFd::create().unpack_ok()), timer_fd_()
{
    poll_instance_.add_fd(stop_fd_.to_weak(), POLLIN);

    // Set initial expiration to lowest possible value, this together with TFD_TIMER_ABSTIME should
    // synchronize our timers
    if (read_interval.count() != 0)
    {
        timer_fd_ = std::make_unique<TimerFd>(std::move(TimerFd::create(read_interval).unpack_ok()));
        poll_instance_.add_fd(timer_fd_->to_weak(), POLLIN);
    }
}

void PollMonitor::add_fd(WeakFd fd, int events)
{
    poll_instance_.add_fd(fd, events);
}

void PollMonitor::stop()
{
    if (!thread_.joinable())
    {
        Log::warn() << "Cannot stop/join PollMonitor thread not running.";
        return;
    }

    stop_fd_.write(1);
    thread_.join();
}

void PollMonitor::run()
{
    bool stop_requested = false;
    do
    {

        poll_instance_.poll().throw_if_error();
        
        num_wakeups_++;

    for (const auto& pfd : poll_instance_.fds())
    {
        WeakFd fd(pfd.fd);
        if(timer_fd_ && fd == *timer_fd_)
        {
            on_readout_interval();
            timer_fd_.reset();
        }
        else if(fd == stop_fd_)
        {
            on_stop();
            Log::debug() << "Requested stop of PollMonitor";
            stop_requested = true;
        }
        else if (pfd.revents != 0)
        {
            on_fd_ready(fd, pfd.revents);
        }
    }

    } while (!stop_requested);
}
} // namespace monitor
} // namespace lo2s
