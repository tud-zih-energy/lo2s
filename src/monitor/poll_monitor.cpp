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

#include <cmath>
extern "C"
{
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
}

namespace lo2s
{
namespace monitor
{
PollMonitor::PollMonitor(trace::Trace& trace, const std::string& name,
                         std::chrono::nanoseconds read_interval)
: ThreadedMonitor(trace, name)
{
    int fds[2];
    int ret = pipe(fds);

    if (ret == -1)
    {
        throw_errno();
    }

    read_fd_ = std::make_optional<Fd>(fds[0]);
    write_fd_ = std::make_optional<Fd>(fds[1]);

    epoll_fd_ = std::optional<Fd>(epoll_create1(0));

    if (!epoll_fd_)
    {
        throw_errno();
    }
    add_fd(*read_fd_);

    // Create and initialize timer_fd
    struct itimerspec tspec;
    memset(&tspec, 0, sizeof(struct itimerspec));

    // Set initial expiration to lowest possible value, this together with TFD_TIMER_ABSTIME should
    // synchronize our timers
    if (read_interval.count() != 0)
    {

        tspec.it_value.tv_nsec = 1;

        tspec.it_interval.tv_sec =
            std::chrono::duration_cast<std::chrono::seconds>(read_interval).count();

        tspec.it_interval.tv_nsec = (read_interval % std::chrono::seconds(1)).count();

        int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (timer_fd == -1)
        {
            throw_errno();
        }

        timer_fd_ = std::make_optional<Fd>(timer_fd);
        add_fd(*timer_fd_);

        if (timerfd_settime(timer_fd_->as_int(), TFD_TIMER_ABSTIME, &tspec, NULL) == -1)
        {
            throw_errno();
        }
    }
}

void PollMonitor::add_fd(const Fd& fd)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(epoll_event));

    ev.events = EPOLLIN;
    ev.data.fd = fd.as_int();

    if (epoll_ctl(epoll_fd_->as_int(), EPOLL_CTL_ADD, fd.as_int(), &ev) == -1)
    {
        throw_errno();
    }

    num_fds_++;
}

void PollMonitor::stop()
{
    if (!thread_.joinable())
    {
        Log::warn() << "Cannot stop/join PollMonitor thread not running.";
        return;
    }

    auto buf = std::byte(0);
    ::write(write_fd_->as_int(), &buf, 1);
    thread_.join();
}

void PollMonitor::run()
{
    std::vector<struct epoll_event> events(num_fds_);
    bool stop_requested = false;
    do
    {
        auto nfds = epoll_wait(epoll_fd_->as_int(), events.data(), num_fds_, -1);
        num_wakeups_++;

        if (nfds == 0)
        {
            throw std::runtime_error("Received poll timeout despite requesting no timeout.");
        }
        else if (nfds < 0)
        {
            // Received Ctrl-C during epoll, ignore as it is harmless
            if (errno == EINTR)
            {
                continue;
            }
            Log::error() << "poll failed";
            throw_errno();
        }
        Log::trace() << "PollMonitor poll returned " << nfds;

        bool panic = false;
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].events != 0 && events[i].events != POLLIN)
            {
                Log::warn() << "Poll on raw event fds got unexpected event flags: "
                            << events[i].events << ". Stopping raw event polling.";
                panic = true;
            }

            // Flush timer
            if (timer_fd_ && events[i].data.fd == timer_fd_->as_int())
            {
                [[maybe_unused]] uint64_t expirations;
                if (read(timer_fd_->as_int(), &expirations, sizeof(expirations)) == -1)
                {
                    Log::error() << "Flushing timer fd failed";
                    throw_errno();
                }
            }
            if (events[i].data.fd == read_fd_->as_int())
            {
                stop_requested = true;
            }
            monitor(events[i].data.fd);
        }
        if (panic)
        {
            break;
        }

    } while (!stop_requested);
}

} // namespace monitor
} // namespace lo2s
