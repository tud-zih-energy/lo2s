#pragma once
#include "lo2s/helpers/maybe_error.hpp"
#include <lo2s/helpers/errno_error.hpp>
#include <lo2s/perf/event_attr.hpp>
namespace lo2s
{
    namespace perf
    {
/**
 * Contains an opened instance of Event.
 * Use any Event.open() method to construct an object
 */
class EventGuard
{
public:
    EventGuard(const EventGuard& other) = delete;
    EventGuard& operator=(const EventGuard&) = delete;

    EventGuard(EventGuard&& other)
    {
        std::swap(fd_, other.fd_);
    }

    EventGuard& operator=(EventGuard&& other)
    {
        std::swap(fd_, other.fd_);
        return *this;
    }

    static Expected<EventGuard, ErrnoError> open(EventAttr& ev, ExecutionScope location, WeakFd group_fd = WeakFd::make_invalid(), WeakFd cgroup_fd = WeakFd::make_invalid());

    /**
     * opens child as a counter of the calling (leader) event
     */
    Expected<EventGuard, ErrnoError> open_child(EventAttr& child, ExecutionScope location);

    MaybeError<ErrnoError> enable();
    MaybeError<ErrnoError> disable();

    Expected<uint64_t, ErrnoError> get_id()
    {
        return fd_.get_stream_id();
    }

    MaybeError<ErrnoError> set_output(const EventGuard& other_ev);
    MaybeError<ErrnoError> set_syscall_filter(const std::vector<int64_t>& filter);

    WeakFd get_weak_fd() const
    {
        return fd_.to_weak();
    }

    template <class T>
    Expected<T, ErrnoError> read()
    {
        return fd_.read<T>();
    }

protected:
    EventGuard() {};
    PerfEventFd fd_;
};

}}
