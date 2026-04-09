// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/rb/writer.hpp>

#include <lo2s/rb/header.hpp>
#include <lo2s/rb/shm_ringbuf.hpp>
#include <lo2s/types/process.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

#include "lo2s/error.hpp"

extern "C"
{
#include <sys/socket.h>
#include <sys/un.h>
}

namespace lo2s
{
RingbufWriter::RingbufWriter(Process process, RingbufMeasurementType type)

{
    char const* socket_path = getenv("LO2S_SOCKET");
    if (socket_path == nullptr)
    {
        throw std::runtime_error("LO2S_SOCKET is not set!");
    }

    int const data_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (data_socket == -1)
    {
        throw std::system_error(errno, std::system_category());
    }

    struct sockaddr_un addr; // NOLINT
    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    int const ret =
        connect(data_socket, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
    if (ret < 0)
    {
        throw std::system_error(errno, std::system_category());
    }

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
    uint64_t payload = 42;
    struct iovec iov[1];
    iov[0].iov_base = &payload;
    iov[0].iov_len = 8;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if (recvmsg(data_socket, &msg, 0) < 0)
    {
        throw_errno();
    }

    struct cmsghdr* cmptr = CMSG_FIRSTHDR(&msg);
    if (cmptr != nullptr && cmptr->cmsg_len == CMSG_LEN(sizeof(int)))
    {
        if (cmptr->cmsg_level != SOL_SOCKET)
        {
            throw std::runtime_error("cmsg_level is not SOL_SOCKET!");
        }
        if (cmptr->cmsg_type != SCM_RIGHTS)
        {
            throw std::runtime_error("cmsg_type is not SCM_RIGHTS!");
        }

        const int recvfd = *reinterpret_cast<int*>(CMSG_DATA(cmptr));
        rb_ = std::make_unique<ShmRingbuf>(recvfd);

        rb_->header()->pid = process.as_int();
        rb_->header()->type = (uint64_t)type;
    }
    else
    {
        throw std::runtime_error("Message does not contain control messages!");
    }
}

void RingbufWriter::commit()
{
    assert(reserved_size_ != 0);
    rb_->advance_head(reserved_size_);
    reserved_size_ = 0;
}

uint64_t RingbufWriter::timestamp()
{
    constexpr uint64_t NSEC_IN_SEC = 1000000000;
    struct timespec ts; // NOLINT
    clock_gettime(rb_->header()->clockid, &ts);
    uint64_t const res = (ts.tv_sec * NSEC_IN_SEC) + ts.tv_nsec;
    return res;
}

} // namespace lo2s
