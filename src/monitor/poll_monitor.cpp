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

extern "C"
{
#include <sys/timerfd.h>
}

namespace lo2s
{
namespace monitor
{
PollMonitor::PollMonitor(trace::Trace& trace, const std::string& name)
: ThreadedMonitor(trace, name)
{
    pfds_.resize(2);
    stop_pfd().fd = stop_pipe_.read_fd();
    stop_pfd().events = POLLIN;
    stop_pfd().revents = 0;

    // Create and initialize timer_fd
    struct itimerspec tspec;
    memset(&tspec, 0, sizeof(struct itimerspec));

    // Set initial expiration to lowest possible value, this together with TFD_TIMER_ABSTIME should
    // synchronize our timers
    tspec.it_value.tv_nsec = 1;
    tspec.it_interval.tv_nsec = config().read_interval.count();

    timer_pfd().fd = timerfd_create(CLOCK_MONOTONIC, 0);
    timer_pfd().events = POLLIN;
    timer_pfd().revents = 0;

    timerfd_settime(timer_pfd().fd, TFD_TIMER_ABSTIME, &tspec, NULL);
}

void PollMonitor::add_fd(int fd)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pfds_.push_back(pfd);
}

void PollMonitor::stop()
{
    if (!thread_.joinable())
    {
        return;
    }

    stop_pipe_.write();
    thread_.join();
}

void PollMonitor::monitor()
{
    if (timer_pfd().revents & POLLIN || stop_pfd().revents & POLLIN)
    {
        monitor(-1);
    }
    for (const auto& pfd : pfds_)
    {
        if (pfd.fd != timer_pfd().fd && pfd.fd != stop_pfd().fd && pfd.revents & POLLIN)
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
        Log::debug() << "PollMonitor poll returned " << ret;

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

        // Flush timer
        if (timer_pfd().revents & POLLIN)
        {
            uint64_t expirations;
            if (read(timer_pfd().fd, &expirations, sizeof(expirations)) == -1)
            {
                Log::error() << "Flushing timer fd failed";
                throw_errno();
            }
            (void)expirations;
        }
        if (stop_pfd().revents & POLLIN)
        {
            Log::debug() << "Requested stop of PollMonitor";
            stop_requested = true;
        }
    } while (!stop_requested);
}
} // namespace monitor
} // namespace lo2s
