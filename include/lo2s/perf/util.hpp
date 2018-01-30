#pragma once

#include <lo2s/util.hpp>

extern "C" {
#include <syscall.h>
}

namespace lo2s
{
namespace perf
{
static inline int perf_event_paranoid()
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

static inline int perf_event_open(struct perf_event_attr* perf_attr, pid_t tid, int cpu,
                                  int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, perf_attr, tid, cpu, group_fd, flags);
}
}
}
