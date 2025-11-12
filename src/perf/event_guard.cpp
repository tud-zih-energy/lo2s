#include "lo2s/helpers/maybe_error.hpp"
#include <lo2s/perf/event_guard.hpp>

namespace lo2s
{
namespace perf {
Expected<EventGuard, ErrnoError> EventGuard::open_child(EventAttr& child, ExecutionScope location)
{
    return open(child, location, fd_.to_weak(), WeakFd::make_invalid());
}

Expected<EventGuard, ErrnoError> EventGuard::open(EventAttr& ev, ExecutionScope location, WeakFd group_fd,
                       WeakFd cgroup_fd)

{

    EventGuard guard;
    Log::trace() << "Opening perf event: " << ev.name() << "[" << location.name()
                 << ", group fd: " << group_fd << ", cgroup fd: " << cgroup_fd << "]";
    Log::trace() << ev;
    
    auto res =  PerfEventFd::create(&ev.attr(), location, group_fd,0, cgroup_fd);
    if (!res.ok())
    {
        Log::trace() << "Couldn't open event!: " << strerror(errno);
        return Unexpected(res.unpack_error());
    }
    guard.fd_ = res.unpack_ok();

    auto nb_res = guard.fd_.make_nonblock()
        ;
    if (!nb_res.ok())
    {
        Log::trace() << "Couldn't set event nonblocking: " << strerror(errno);
        return Unexpected(nb_res.unpack_error());
    }
    Log::trace() << "Succesfully opened perf event! fd: " << guard.fd_;

    return guard;
}

MaybeError<ErrnoError> EventGuard::enable()
{
    return fd_.enable();
}

MaybeError<ErrnoError> EventGuard::disable()
{
    return fd_.disable();
}

MaybeError<ErrnoError> EventGuard::set_output(const EventGuard& other_ev)
{
    return fd_.set_output_to(other_ev.get_weak_fd());
}

MaybeError<ErrnoError> EventGuard::set_syscall_filter(const std::vector<int64_t>& syscall_filter)
{
    return fd_.set_syscall_filter(syscall_filter);
}

}
}
