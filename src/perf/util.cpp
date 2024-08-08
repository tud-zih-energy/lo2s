#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event.hpp>
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

void perf_warn_paranoid()
{
    static bool warning_issued = false;

    if (!warning_issued)
    {
        Log::warn() << "You requested kernel sampling, but kernel.perf_event_paranoid > 1, "
                       "retrying without kernel samples.";
        Log::warn() << "To solve this warning you can do one of the following:";
        Log::warn() << " * sysctl kernel.perf_event_paranoid=1";
        Log::warn() << " * run lo2s as root";
        Log::warn() << " * run with --no-kernel to disable kernel space sampling in "
                       "the first place,";
        warning_issued = true;
    }
}

void perf_check_disabled()
{
    if (perf_event_paranoid() == 3)
    {
        Log::error() << "kernel.perf_event_paranoid is set to 3, which disables perf altogether.";
        Log::warn() << "To solve this error, you can do one of the following:";
        Log::warn() << " * sysctl kernel.perf_event_paranoid=2";
        Log::warn() << " * run lo2s as root";

        throw std::runtime_error("Perf is disabled via a paranoid setting of 3.");
    }
}
} // namespace perf
} // namespace lo2s
