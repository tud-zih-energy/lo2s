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

#include "lo2s/ringbuf.hpp"
#include <fcntl.h>
#include <lo2s/monitor/socket_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/gpu_monitor.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/helpers/fd.hpp>

#include <stdexcept>

extern "C"
{
#include <sys/mman.h>
#include <sys/socket.h>
}

namespace lo2s
{
namespace monitor
{
SocketMonitor::SocketMonitor(trace::Trace& trace)
: PollMonitor(trace, "SocketMonitor", std::chrono::nanoseconds(0)), trace_(trace), socket_(UnixDomainSocket::create(config().socket_path).unpack_ok())
{
    add_fd(socket_.to_weak(), POLLIN);
}

void SocketMonitor::finalize_thread()
{
    for (auto& monitor : gpu_monitors_)
    {
        monitor.second.stop();
    }

    for (auto& monitor : openmp_monitors_)
    {
        monitor.second.stop();
    }
}

void SocketMonitor::on_fd_ready(WeakFd fd, [[maybe_unused]] int revents)
{
    if (fd != socket_)
    {
        if (gpu_monitors_.count(fd))
        {
            gpu_monitors_.erase(fd);
        }
        else if (openmp_monitors_.count(fd))
        {
            openmp_monitors_.erase(fd);
        }
        else
        {
            throw std::runtime_error(fmt::format("fd was never seen before: {}!", fd));
        }
    }
    else
    {
        auto child_socket = socket_.accept().unpack_ok();

        add_fd(child_socket.to_weak(), 0);

        auto read_fd_res = child_socket.read_fd();

        if (read_fd_res.ok())
        {
            auto read_fd = read_fd_res.unpack_ok();
            if (static_cast<RingbufMeasurementType>(read_fd.first) == RingbufMeasurementType::GPU)
            {
                auto res = gpu_monitors_.emplace(
                    std::piecewise_construct, std::forward_as_tuple(child_socket.to_weak()),
                    std::forward_as_tuple(trace_, std::move(read_fd.second)));
                res.first->second.start();
            }
            else if (static_cast<RingbufMeasurementType>(read_fd.first) ==
                     RingbufMeasurementType::OPENMP)

            {
                auto res = openmp_monitors_.emplace(
                    std::piecewise_construct, std::forward_as_tuple(child_socket.to_weak()),
                    std::forward_as_tuple(trace_, std::move(read_fd.second)));
                res.first->second.start();
            }
            else
            {
                throw std::runtime_error(fmt::format("Invalid ring buffer measurement type: {}",
                                                     static_cast<uint64_t>(read_fd.first)));
            }
        }
    }
}

void SocketMonitor::on_readout_interval()
{
    throw std::runtime_error("SocketMonitor did not specify a readout interval, but on_readout_intervall still triggered!");
}

void SocketMonitor::on_stop()
{
}

} // namespace monitor
} // namespace lo2s
