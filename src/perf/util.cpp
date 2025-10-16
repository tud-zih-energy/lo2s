#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/util.hpp>

extern "C"
{
#include <linux/perf_event.h>
#include <syscall.h>
}

namespace lo2s
{
namespace perf
{
int perf_event_paranoid()
{
    try
    {
        return get_sysctl<int>("kernel", "perf_event_paranoid");
    }
    catch (...)
    {
        Log::warn() << "Failed to access kernel.perf_event_paranoid. Assuming 2.";
        return 2;
    }
}

int perf_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope, int group_fd,
                    unsigned long flags, int cgroup_fd)
{
    assert(config_or_default().use_perf());
    int cpuid = -1;
    pid_t pid = -1;
    if (scope.is_cpu())
    {
        if (cgroup_fd != -1)
        {
            pid = cgroup_fd;
            flags |= PERF_FLAG_PID_CGROUP;
        }
        cpuid = scope.as_cpu().as_int();
    }
    else
    {
        pid = scope.as_thread().as_pid_t();
    }
    return syscall(__NR_perf_event_open, perf_attr, pid, cpuid, group_fd, flags);
}

} // namespace perf
} // namespace lo2s
