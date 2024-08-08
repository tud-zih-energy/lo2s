#pragma once

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{
namespace perf
{

int perf_event_paranoid();
int perf_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope, int group_fd,
                    unsigned long flags, int cgroup_fd = -1);
void perf_warn_paranoid();
void perf_check_disabled();
} // namespace perf
} // namespace lo2s
