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
#include <lo2s/log.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/time/time.hpp>

#include <memory>

extern "C"
{
#include <sys/mman.h>
#include <sys/socket.h>
}

namespace lo2s
{
namespace monitor
{
SocketMonitor::SocketMonitor(trace::Trace& trace, MainMonitor& main_monitor)
: PollMonitor(trace, "SocketMonitor", std::chrono::nanoseconds(0)), trace_(trace),
  main_monitor_(main_monitor)
{
    socket = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (socket == -1)
    {
        throw_errno();
    }

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, config().socket_path.c_str(), sizeof(name.sun_path) - 1);
    unlink(config().socket_path.c_str());

    int ret = bind(socket, (const struct sockaddr*)&name, sizeof(name));
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

static ssize_t read_fd(int fd, int* recvfd)
{
    ssize_t n;

    union
    {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    struct msghdr msg;
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    // We have to send at least one byte with the fd, otherwise we can not distinguish receiving
    // messages from receiving EOF
    char sentinel;
    struct iovec iov[1];
    iov[0].iov_base = &sentinel;
    iov[0].iov_len = 1;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if ((n = recvmsg(fd, &msg, 0)) <= 0)
    {
        throw_errno();
    }

    assert(sentinel == 42);
    struct cmsghdr* cmptr;
    if ((cmptr = CMSG_FIRSTHDR(&msg)) != nullptr && cmptr->cmsg_len == CMSG_LEN(sizeof(int)))
    {
        if (cmptr->cmsg_level != SOL_SOCKET)
            return -1;
        if (cmptr->cmsg_type != SCM_RIGHTS)
            return -1;

        *recvfd = *((int*)CMSG_DATA(cmptr));
    }
    else
    {
        *recvfd = -1;
    }
    return n;
}

void SocketMonitor::initialize_thread()
{
}

void SocketMonitor::finalize_thread()
{
    for (auto& monitor : monitors_)
    {
        monitor.second.stop();
    }
    close(socket);
}

void SocketMonitor::monitor(int fd)
{
    if (fd != socket)
    {
        return;
    }
    int data_socket = accept(socket, NULL, NULL);
    if (data_socket == -1)
    {
        throw_errno();
    }

    int foo_fd = -1;
    if (read_fd(data_socket, &foo_fd) <= 0)
    {
        std::cout << "Couldnt read fd: " << strerror(errno) << std::endl;
    }

    auto res = monitors_.emplace(std::piecewise_construct, std::forward_as_tuple(foo_fd),
                                 std::forward_as_tuple(trace_, main_monitor_, foo_fd));
    res.first->second.start();
    close(data_socket);
}
} // namespace monitor
} // namespace lo2s
