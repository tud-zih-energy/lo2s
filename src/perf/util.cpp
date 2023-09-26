#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_description.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/util.hpp>
#include <tuple>

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

std::optional<Fd> perf_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope,
                                  const std::optional<Fd>& group_fd, unsigned long flags,
                                  const std::optional<Fd>& cgroup_fd)
{
    int cpuid = -1;
    pid_t pid = -1;
    if (scope.is_cpu())
    {
        if (cgroup_fd)
        {
            pid = cgroup_fd->as_int();
            flags |= PERF_FLAG_PID_CGROUP;
        }
        cpuid = scope.as_cpu().as_int();
    }
    else
    {
        pid = scope.as_thread().as_pid_t();
    }

    int group_fd_int = -1;
    if (group_fd)
    {
        group_fd_int = group_fd->as_int();
    }

    int fd = syscall(__NR_perf_event_open, perf_attr, pid, cpuid, group_fd_int, flags);

    if (fd != -1)
    {
        return std::make_optional<Fd>(fd);
    }
    return std::optional<Fd>();
}

// Default options we use in every perf_event_open call
struct perf_event_attr common_perf_event_attrs()
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));

    attr.size = sizeof(struct perf_event_attr);
    attr.disabled = 1;

#ifndef USE_HW_BREAKPOINT_COMPAT
    attr.use_clockid = config().use_clockid;
    attr.clockid = config().clockid;
#endif
    // When we poll on the fd given by perf_event_open, wakeup, when our buffer is 80% full
    // Default behaviour is to wakeup on every event, which is horrible performance wise
    attr.watermark = 1;
    attr.wakeup_watermark = static_cast<uint32_t>(0.8 * config().mmap_pages * get_page_size());

    return attr;
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
std::optional<Fd> perf_try_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope,
                                      const std::optional<Fd>& group_fd, unsigned long flags,
                                      const std::optional<Fd>& cgroup_fd)
{
    auto fd = perf_event_open(perf_attr, scope, group_fd, flags, cgroup_fd);
    if (!fd && errno == EACCES && !perf_attr->exclude_kernel && perf_event_paranoid() > 1)
    {
        perf_attr->exclude_kernel = 1;
        perf_warn_paranoid();
        fd = perf_event_open(perf_attr, scope, group_fd, flags, cgroup_fd);
    }
    return fd;
}

std::optional<Fd> perf_event_description_open(ExecutionScope scope, const EventDescription& desc,
                                              const std::optional<Fd>& group_fd)
{
    struct perf_event_attr perf_attr;
    memset(&perf_attr, 0, sizeof(perf_attr));
    perf_attr.size = sizeof(perf_attr);
    perf_attr.sample_period = 0;
    perf_attr.type = desc.type;
    perf_attr.config = desc.config;
    perf_attr.config1 = desc.config1;
    perf_attr.exclude_kernel = config().exclude_kernel;
    // Needed when scaling multiplexed events, and recognize activation phases
    perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

#ifndef USE_HW_BREAKPOINT_COMPAT
    perf_attr.use_clockid = config().use_clockid;
    perf_attr.clockid = config().clockid;
#endif

    auto fd = perf_try_event_open(&perf_attr, scope, group_fd, 0, config().cgroup_fd);
    if (!fd)
    {
        Log::error() << "perf_event_open for counter failed";
        throw_errno();
    }
    return fd;
}
} // namespace perf
} // namespace lo2s
