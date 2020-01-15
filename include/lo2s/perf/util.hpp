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
int perf_event_open(struct perf_event_attr* perf_attr, pid_t tid, int cpu, int group_fd,
                    unsigned long flags);
struct perf_event_attr common_perf_event_attrs();
void perf_warn_paranoid();

} // namespace perf
} // namespace lo2s
