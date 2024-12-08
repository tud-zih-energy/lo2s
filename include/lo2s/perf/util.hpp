#pragma once

#include <lo2s/perf/event_description.hpp>

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
struct perf_event_attr common_perf_event_attrs();
void perf_warn_paranoid();
void perf_check_disabled();

int perf_event_description_open(ExecutionScope scope, const EventDescription& desc, int group_fd);
int perf_try_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope, int group_fd,
                        unsigned long flags, int cgroup_fd = -1);

} // namespace perf
} // namespace lo2s
