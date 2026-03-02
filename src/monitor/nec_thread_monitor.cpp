// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/nec_thread_monitor.hpp>

#include <chrono>

#include <cassert>

extern "C"
{
#include <libved.h>
#include <veosinfo/veosinfo.h>
}

namespace lo2s
{
namespace nec
{
NecThreadMonitor::NecThreadMonitor(Thread thread, trace::Trace& trace, NecDevice device)
: PollMonitor(trace, fmt::format("VE{} {}", device, thread.as_pid_t()),
              std::chrono::duration_cast<std::chrono::nanoseconds>(config().nec_read_interval)),
  perf::counter::MetricWriter(MeasurementScope::nec_metric(thread.as_scope()), trace),
  nec_read_interval_(config().nec_read_interval),
  otf2_writer_(trace.sample_writer(MeasurementScope::nec_sample(ExecutionScope(thread)))),
  nec_thread_(thread), trace_(trace), device_(device), cctx_manager_(trace)
{
    cctx_manager_.thread_enter(nec_thread_.as_process(), thread);
    otf2_writer_.write_calling_context_enter(lo2s::time::now(), cctx_manager_.current(), 2);
}

void NecThreadMonitor::monitor([[maybe_unused]] int fd)
{
    static int reg[] = {
        VE_USR_PMC00, VE_USR_PMC01, VE_USR_PMC02, VE_USR_PMC03, VE_USR_PMC04, VE_USR_PMC05,
        VE_USR_PMC06, VE_USR_PMC07, VE_USR_PMC08, VE_USR_PMC09, VE_USR_PMC10, VE_USR_PMC11,
        VE_USR_PMC12, VE_USR_PMC13, VE_USR_PMC14, VE_USR_PMC15, VE_USR_IC,
    };

    constexpr size_t num_counters = sizeof(reg) / sizeof(int);

    uint64_t val[num_counters];

    assert(reg[num_counters - 1] == VE_USR_IC);

    auto ret = ve_get_regvals(device_.as_int(), nec_thread_.as_pid_t(), num_counters, reg, val);

    if (ret == -1)
    {
        Log::error() << "Failed to the vector engine instruction counter value!";
        throw_errno();
    }

    otf2::chrono::time_point tp = lo2s::time::now();
    otf2_writer_.write_calling_context_sample(tp, cctx_manager_.sample_ref(val[num_counters - 1]),
                                              2, trace_.nec_interrupt_generator().ref());

    metric_event_.timestamp(tp);

    otf2::event::metric::values& values = metric_event_.raw_values();

    for (size_t i = 0; i < num_counters - 1; i++)
    {
        values[i] = val[i];
    }
    writer_.write(metric_event_);
}

void NecThreadMonitor::finalize_thread()
{
    if (!cctx_manager_.current().is_undefined())
    {
        otf2_writer_.write_calling_context_leave(lo2s::time::now(), cctx_manager_.current());
    }

    cctx_manager_.finalize(&otf2_writer_);
}
} // namespace nec
} // namespace lo2s
