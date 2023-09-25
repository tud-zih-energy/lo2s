#pragma once

#include <lo2s/perf/event_description.hpp>
#include <lo2s/types/fd.hpp>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{
namespace perf
{

int perf_event_paranoid();

Fd perf_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope,
                   const Fd& group_fd = Fd::invalid(), unsigned long flags = 0,
                   const Fd& cgroup_fd = Fd::invalid());
struct perf_event_attr common_perf_event_attrs();
void perf_warn_paranoid();
void perf_check_disabled();

Fd perf_event_description_open(ExecutionScope scope, const EventDescription& desc,
                               const Fd& group_fd = Fd::invalid());
Fd perf_try_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope,
                       const Fd& group_fd = Fd::invalid(), unsigned long flags = 0,
                       const Fd& cgroup_fd = Fd::invalid());

} // namespace perf
} // namespace lo2s
