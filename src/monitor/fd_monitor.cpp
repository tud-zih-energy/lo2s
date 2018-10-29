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

#include <lo2s/monitor/fd_monitor.hpp>

#include <nitro/lang/enumerate.hpp>

namespace lo2s
{
namespace monitor
{
FdMonitor::FdMonitor(trace::Trace& trace, const std::string& name) : ThreadedMonitor(trace, name)
{
    pfds_.resize(1);
    stop_pfd().fd = stop_pipe_.read_fd();
    stop_pfd().events = POLLIN;
    stop_pfd().revents = 0;
}

size_t FdMonitor::add_fd(int fd)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pfds_.push_back(pfd);
    return pfds_.size() - 2;
}

void FdMonitor::stop()
{
    stop_pipe_.write();
    if (!thread_.joinable())
    {
        Log::error() << "Cannot stop/join FdMonitor thread not running.";
    }
    thread_.join();
}

void FdMonitor::monitor()
{
    for (const auto& index_pfd : nitro::lang::enumerate(pfds_))
    {
        if (index_pfd.index() == 0)
        {
            continue;
        }
        if (index_pfd.value().revents & POLLIN || stop_pfd().revents & POLLIN)
        {
            monitor(index_pfd.index() - 1);
        }
    }
}

void FdMonitor::run()
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
        Log::debug() << "FdMonitor poll returned " << ret;

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
            Log::debug() << "Requested stop of FdMonitor";
            stop_requested = true;
        }
    } while (!stop_requested);
}
} // namespace monitor
} // namespace lo2s
