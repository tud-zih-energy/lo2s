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

#include <lo2s/monitor/socket_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/monitor/gpu_monitor.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/rb/header.hpp>
#include <lo2s/trace/trace.hpp>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <cstdint>
#include <cstring>

#include <fmt/format.h>

extern "C"
{
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
}

namespace lo2s::monitor
{
SocketMonitor::SocketMonitor(trace::Trace& trace)
: PollMonitor(trace, "SocketMonitor", std::chrono::nanoseconds(0)), trace_(trace),
  socket(::socket(AF_UNIX, SOCK_SEQPACKET, 0))
{
    if (socket == -1)
    {
        throw_errno();
    }

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, config().socket_path.c_str(), sizeof(name.sun_path) - 1);

    unlink(config().socket_path.c_str());
    int ret = bind(socket, reinterpret_cast<const struct sockaddr*>(&name), sizeof(name));
    if (ret == -1)
    {
        throw_errno();
    }

    ret = listen(socket, 20);
    if (ret == -1)
    {
        throw_errno();
    }
    add_fd(socket);
}

namespace
{
std::optional<std::pair<RingbufMeasurementType, int>> read_fd(int socket)
{
    union
    {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    struct msghdr msg;
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    msg.msg_name = nullptr;
    msg.msg_namelen = 0;

    // We have to send a message together with the fd, so use that to send the type of the
    // connected measurement
    RingbufMeasurementType type = RingbufMeasurementType::GPU;
    struct iovec iov[1];
    iov[0].iov_base = &type;
    iov[0].iov_len = 8;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if (recvmsg(socket, &msg, 0) <= 0)
    {
        throw_errno();
    }

    struct cmsghdr* cmptr = CMSG_FIRSTHDR(&msg);
    if (cmptr != nullptr && cmptr->cmsg_len == CMSG_LEN(sizeof(int)))
    {
        if (cmptr->cmsg_level != SOL_SOCKET)
        {
            return std::nullopt;
        }
        if (cmptr->cmsg_type != SCM_RIGHTS)
        {
            return std::nullopt;
        }

        const int recvfd = *reinterpret_cast<int*>(CMSG_DATA(cmptr));
        return std::pair<RingbufMeasurementType, int>(type, recvfd);
    }
    return std::nullopt;
}
} // namespace

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

    close(socket);
    unlink(config().socket_path.c_str());
}

void SocketMonitor::monitor(int fd)
{
    if (fd != socket)
    {
        return;
    }
    const int data_socket = accept(socket, nullptr, nullptr);
    if (data_socket == -1)
    {
        throw_errno();
    }

    int foo_fd = -1;
    auto type_fd = read_fd(data_socket);

    if (type_fd.has_value())
    {
        if (type_fd.value().first == RingbufMeasurementType::GPU)
        {
            auto res =
                gpu_monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(foo_fd),
                                      std::forward_as_tuple(trace_, type_fd.value().second));
            res.first->second.start();
        }
        else if (type_fd.value().first == RingbufMeasurementType::OPENMP)

        {
            auto res =
                openmp_monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(foo_fd),
                                         std::forward_as_tuple(trace_, type_fd.value().second));
            res.first->second.start();
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid ring buffer measurement type: {}",
                                                 static_cast<uint64_t>(type_fd.value().first)));
        }
    }
    close(data_socket);
}
} // namespace lo2s::monitor
