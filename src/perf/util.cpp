#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
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

int perf_event_open(struct perf_event_attr* perf_attr, pid_t tid, int cpu, int group_fd,
                    unsigned long flags)
{
    return syscall(__NR_perf_event_open, perf_attr, tid, cpu, group_fd, flags);
}

// Default options we use in every perf_event_open call
struct perf_event_attr common_perf_event_attrs()
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));

    attr.size = sizeof(struct perf_event_attr);
    attr.disabled = 1;

#if !defined(USE_HW_BREAKPOINT_COMPAT) && defined(USE_PERF_CLOCKID)
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
} // namespace perf
} // namespace lo2s
