#pragma once

#include <lo2s/execution_scope.hpp>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf
{
int perf_event_paranoid();
int perf_event_open(const struct perf_event_attr* perf_attr, ExecutionScope scope, int group_fd,
                    unsigned long flags, int cgroup_fd = -1);
} // namespace lo2s::perf
