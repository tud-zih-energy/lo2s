#pragma once

#include <cstdint>

#include <lo2s/helpers/fd.hpp>
#include <lo2s/helpers/expected.hpp>
#include <lo2s/helpers/errno_error.hpp>
extern "C"
{
#include <sys/un.h>
#include <sys/socket.h>
}

namespace lo2s
{
class UnixDomainSocket : public Fd
{
    public:
    static Expected<UnixDomainSocket, ErrnoError> create(std::string socket_path)
    {
      UnixDomainSocket socket;  
    socket.fd_ = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (socket.invalid())
    {
        return Unexpected(ErrnoError::from_errno());
    }

    struct sockaddr_un name;
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, socket_path.c_str(), sizeof(name.sun_path) - 1);

    unlink(socket_path.c_str());
    if(bind(socket.fd_, (const struct sockaddr*)&name, sizeof(name)) == -1)
    {
        return Unexpected(ErrnoError::from_errno());
    }

    if(listen(socket.fd_, 20) == -1)
    {
    }

    return socket;
    }

    Expected<UnixDomainSocket, ErrnoError> accept()
    {
        UnixDomainSocket child;
        child.fd_ = ::accept(fd_, NULL, NULL);

        if(child.fd_ == -1)
        {
            return Unexpected(ErrnoError::from_errno());
        }
        return child;
        
    }

    Expected<std::pair<uint64_t, Fd>, ErrnoError> read_fd()
    {

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

    // We have to send a message together with the fd, so use that to send the type of the
    // connected measurement
    uint64_t type;
    struct iovec iov[1];
    iov[0].iov_base = &type;
    iov[0].iov_len = 8;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if (recvmsg(fd_, &msg, 0) == -1)
    {
        return Unexpected(ErrnoError::from_errno());
    }

    struct cmsghdr* cmptr;
    if ((cmptr = CMSG_FIRSTHDR(&msg)) != nullptr && cmptr->cmsg_len == CMSG_LEN(sizeof(int)))
        {
            if (cmptr->cmsg_level != SOL_SOCKET)
                return Unexpected(ErrnoError(-1, "Retrieving fd from message failed!"));
            if (cmptr->cmsg_type != SCM_RIGHTS)
                return Unexpected(ErrnoError(-1, "Retrieving fd from message failed!"));

            return std::pair<uint64_t, Fd>{type, Fd::from_int(*((int*)CMSG_DATA(cmptr)))};
        }

        return Unexpected(ErrnoError(-1, "Retrieving fd from message failed!"));
    }
    private:
    UnixDomainSocket() = default;
};
}
