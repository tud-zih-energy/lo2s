#pragma once

#include <lo2s/perf/event_description.hpp>
#include <lo2s/types/fd.hpp>

#include <optional>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{
namespace perf
{

int perf_event_paranoid();

std::optional<Fd> perf_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope,
                                  const std::optional<Fd>& group_fd = std::optional<Fd>(),
                                  unsigned long flags = 0,
                                  const std::optional<Fd>& cgroup_fd = std::optional<Fd>());
struct perf_event_attr common_perf_event_attrs();
void perf_warn_paranoid();
void perf_check_disabled();

std::optional<Fd>
perf_event_description_open(ExecutionScope scope, const EventDescription& desc,
                            const std::optional<Fd>& group_fd = std::optional<Fd>());

std::optional<Fd> perf_try_event_open(struct perf_event_attr* perf_attr, ExecutionScope scope,
                                      const std::optional<Fd>& group_fd = std::optional<Fd>(),
                                      unsigned long flags = 0,
                                      const std::optional<Fd>& cgroup_fd = std::optional<Fd>());

} // namespace perf
} // namespace lo2s
