#include <lo2s/perf/util.hpp>

#include <lo2s/log.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/util.hpp>

#include <cassert>

#include <asm/unistd_64.h>

extern "C"
{
#include <linux/perf_event.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>
}

namespace lo2s::perf
{
int perf_event_paranoid()
{
    // Root is equivalent to -1
    if (getuid() == 0)
    {
        return -1;
    }
    try
    {
        // root is equivalent to -1
        if (getuid() == 0)
        {
            return -1;
        }

        return get_sysctl<int>("kernel", "perf_event_paranoid");
    }
    catch (...)
    {
        Log::warn() << "Failed to access kernel.perf_event_paranoid. Assuming 2.";
        return 2;
    }
}

int perf_event_open(const struct perf_event_attr* perf_attr, ExecutionScope scope, int group_fd,
                    unsigned long flags, int cgroup_fd)
{
    int cpuid = -1;
    pid_t pid = -1;
    if (scope.is_cpu())
    {
        if (cgroup_fd != -1)
        {
            pid = cgroup_fd;
            flags |= PERF_FLAG_PID_CGROUP;
        }
        cpuid = static_cast<int>(scope.as_cpu().as_int());
    }
    else
    {
        pid = static_cast<pid_t>(scope.as_thread().as_int());
    }
    return static_cast<int>(syscall(__NR_perf_event_open, perf_attr, pid, cpuid, group_fd, flags));
}

} // namespace lo2s::perf
