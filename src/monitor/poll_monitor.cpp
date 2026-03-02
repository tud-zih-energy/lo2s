// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    if (write(stop_pfd().fd, &stop_val, sizeof(stop_val)) != sizeof(stop_val))
    {
        Log::warn() << "Could not send stop signal: " << strerror(errno);
    }
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
