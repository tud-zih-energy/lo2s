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
: PollMonitor(trace, "SocketMonitor"), trace_(trace), socket_(::socket(AF_UNIX, SOCK_SEQPACKET, 0))
{
    if (socket_ == -1)
    {
        throw_errno();
    }

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, config().rb.socket_path.c_str(), sizeof(name.sun_path) - 1);

    unlink(config().rb.socket_path.c_str());
    int ret = bind(socket_, reinterpret_cast<const struct sockaddr*>(&name), sizeof(name));
    if (ret == -1)
    {
        throw_errno();
    }

    ret = listen(socket_, 20);
    if (ret == -1)
    {
        throw_errno();
    }
    add_fd(socket_);
}

// Writes the fd of the shared memory to the Unix Domain Socket

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

    close(socket_);
    socket_ = -1;
    unlink(config().rb.socket_path.c_str());
}

void SocketMonitor::monitor(int fd)
{
    if (fd != socket_)
    {
        return;
    }

    int data_socket = accept(socket_, nullptr, nullptr);
    if (data_socket == -1)
    {
        throw_errno();
    }

    union // NOLINT
    {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    struct msghdr msg; // NOLINT
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    RingbufReader rr(config().perf.clockid.value());
    struct cmsghdr* cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;

    int send_fd = rr.fd();
    memcpy(CMSG_DATA(cmptr), &send_fd, sizeof(int));

    msg.msg_name = nullptr;
    msg.msg_namelen = 0;

    // We need to send some data with the fd anyways, so use that to send
    // the type of the measurement that we are doing.
    struct iovec iov[1];
    uint64_t payload = 42;
    iov[0].iov_base = &payload;
    iov[0].iov_len = sizeof(uint64_t);

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if (sendmsg(data_socket, &msg, 0) == -1)
    {
        throw_errno();
    }

    while (rr.header()->type == 0)
    {
    }

    if (rr.header()->type == (uint64_t)RingbufMeasurementType::GPU)
    {

        auto res = gpu_monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(rr.fd()),
                                         std::forward_as_tuple(trace_, std::move(rr)));
        res.first->second.start();
    }
    else if (rr.header()->type == (uint64_t)RingbufMeasurementType::OPENMP)

    {
        auto res =
            openmp_monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(rr.fd()),
                                     std::forward_as_tuple(trace_, std::move(rr)));
        res.first->second.start();
    }
    else
    {
        throw std::runtime_error(
            fmt::format("Invalid ring buffer measurement type: {}", rr.header()->type));
    }
    close(data_socket);
}
} // namespace lo2s::monitor
