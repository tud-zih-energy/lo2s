#pragma once 

#include <lo2s/helpers/errno_error.hpp>
#include <lo2s/helpers/expected.hpp>
#include <lo2s/helpers/maybe_error.hpp>
#include <lo2s/helpers/fd.hpp>

extern "C"
{
#include <sys/eventfd.h>
}

namespace lo2s
{
class EventFd : public Fd
{
    public:
        static Expected<EventFd, ErrnoError> create()
        {
            EventFd fd;
            fd.fd_ = eventfd(0,0);

            if(fd.fd_ == -1)
            {
                return Unexpected(ErrnoError::from_errno());
            }

            return fd;
        }
        MaybeError<ErrnoError> write(uint64_t value)
        {
            if(::write(fd_, &value, sizeof(value)) == -1)
            {
                ErrnoError::from_errno();
            }
            return {};
        }
};
}
