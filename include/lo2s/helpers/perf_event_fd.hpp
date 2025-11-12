#pragma once

#include "errno_error.hpp"
#include "lo2s/helpers/errno_error.hpp"
#include "lo2s/helpers/expected.hpp"
#include "maybe_error.hpp"
#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/helpers/fd.hpp>

#include <cstdint>
#include <vector>

extern "C"
{
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
}
namespace lo2s{

    class PerfEventFd : public Fd
    {
        public:
    
    template <class T>
    Expected<T, ErrnoError> read()
    {
        static_assert(std::is_pod_v<T> == true);
        T val;

        if (::read(fd_, &val, sizeof(val)) == -1)
        {
            return Unexpected(ErrnoError::from_errno());
        }

        return val;
    }

    Expected<uint64_t, ErrnoError> get_stream_id()
    {
        uint64_t id;
        if (ioctl(fd_, PERF_EVENT_IOC_ID, &id) == -1)
        {
            return Unexpected(ErrnoError::from_errno());
        }
        return id;
    }
    
    MaybeError<ErrnoError> enable()
    {
        if(ioctl(fd_, PERF_EVENT_IOC_ENABLE) == -1)
        {
            return ErrnoError::from_errno();
        }
        return {};
    }
    
    MaybeError<ErrnoError> disable()
    {
        if(ioctl(fd_, PERF_EVENT_IOC_DISABLE) == -1)
        {
            return ErrnoError::from_errno();
        }
        return {};
    }
    
    static Expected<PerfEventFd, ErrnoError> create(struct perf_event_attr* perf_attr, ExecutionScope scope, WeakFd group_fd,
                    unsigned long flags, WeakFd cgroup_fd)
{
    PerfEventFd res;

    assert(config_or_default().use_perf());
    int cpuid = -1;
    pid_t pid = -1;
    if (scope.is_cpu())
    {
        if (!cgroup_fd.invalid())
        {
            pid = cgroup_fd.as_int();
            flags |= PERF_FLAG_PID_CGROUP;
        }
        cpuid = scope.as_cpu().as_int();
    }
    else
    {
        pid = scope.as_thread().as_pid_t();
    }
    if((res.fd_ = syscall(__NR_perf_event_open, perf_attr, pid, cpuid, group_fd.as_int(), flags)) == -1)
    {
        return Unexpected(ErrnoError::from_errno());
    }

    return  res;
}

MaybeError<ErrnoError> set_output_to(WeakFd other)
{
    if (ioctl(fd_, PERF_EVENT_IOC_SET_OUTPUT, other.as_int()) == -1)
    {
        return ErrnoError::from_errno();
    }
    return {};
}

MaybeError<ErrnoError> set_syscall_filter(const std::vector<int64_t>& syscall_filter)
{
    if (syscall_filter.empty())
    {
        return {};
    }

    std::vector<std::string> names;
    std::transform(syscall_filter.cbegin(), syscall_filter.end(), std::back_inserter(names),
                   [](const auto& elem) { return fmt::format("id == {}", elem); });
    std::string filter = fmt::format("{}", fmt::join(names, "||"));

    if (ioctl(fd_, PERF_EVENT_IOC_SET_FILTER, filter.c_str()) == -1)
    {
        return ErrnoError::from_errno();
    }
    return {};
}
    };
}
