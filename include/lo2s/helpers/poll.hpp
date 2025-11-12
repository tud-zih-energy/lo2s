#pragma once

#include <lo2s/helpers/fd.hpp>
#include <lo2s/helpers/maybe_error.hpp>
#include <lo2s/helpers/errno_error.hpp>

#include <vector>

extern "C"
{
#include <poll.h>
}
namespace lo2s
{
class Poll
{
    public:
    void add_fd(WeakFd fd, short events)
    {
        struct pollfd new_elem;
        new_elem.fd = fd.as_int();
        new_elem.events = events;
        new_elem.revents = events;
        pfds_.emplace_back(new_elem);
    }


    MaybeError<ErrnoError> poll()
    {
        if(::poll(pfds_.data(), pfds_.size(), -1) == -1)
        {
            return ErrnoError::from_errno();
        }
        return {};
    }

    const std::vector<struct pollfd> &fds()
    {
        return pfds_;
    }

    private:
    std::vector<struct pollfd> pfds_;
};
}
