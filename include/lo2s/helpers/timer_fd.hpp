#pragma once

#include "lo2s/helpers/maybe_error.hpp"
#include <lo2s/helpers/fd.hpp>

#include <chrono>
#include <cstdlib>

extern "C" 
{

#include <sys/timerfd.h>
}

namespace lo2s
{
class TimerFd : public Fd
{
    public:
        static Expected<TimerFd, ErrnoError> create(std::chrono::nanoseconds duration)
        {
            TimerFd res;

            struct itimerspec tspec;
            memset(&tspec, 0, sizeof(struct itimerspec));

    tspec.it_value.tv_nsec = 1;
    tspec.it_interval.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    tspec.it_interval.tv_nsec = (duration % std::chrono::seconds(1)).count();

    res.fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (res.fd_ == -1)
    {
        return Unexpected(ErrnoError::from_errno());
    }

    if (timerfd_settime(res.fd_, TFD_TIMER_ABSTIME, &tspec, NULL) == -1)
    {
        return Unexpected(ErrnoError::from_errno());
    }
    return res;
        }

        MaybeError<ErrnoError> reset()
        {

            [[maybe_unused]] uint64_t expirations;
    if (::read(fd_, &expirations, sizeof(expirations)) == -1 && errno != EAGAIN)
    {
        return ErrnoError::from_errno();
    }
    return {};
        }

};
}
