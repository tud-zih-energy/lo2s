// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/socket_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/monitor/gpu_monitor.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/rb/header.hpp>
#include <lo2s/trace/trace.hpp>

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
: PollMonitor(trace, "SocketMonitor"), trace_(trace), socket(::socket(AF_UNIX, SOCK_SEQPACKET, 0))
{
    if (socket == -1)
    {
        throw_errno();
    }

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, config().rb.socket_path.c_str(), sizeof(name.sun_path) - 1);

    unlink(config().rb.socket_path.c_str());
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
    unlink(config().rb.socket_path.c_str());
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
