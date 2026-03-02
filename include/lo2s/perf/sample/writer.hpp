// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/execution_scope.hpp>
#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/perf/sample/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/types.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types/process.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/event/metric.hpp>

namespace lo2s::perf::sample
{

// Note, this cannot be protected for CRTP reasons...
class Writer : public Reader<Writer>
{
public:
    Writer(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec);
    ~Writer();

    Writer(Writer&) = delete;
    Writer(Writer&&) = delete;

    Writer& operator=(Writer&) = delete;
    Writer& operator=(Writer&&) = delete;

    using Reader<Writer>::handle;
    bool handle(const Reader::RecordSampleType* sample);
    bool handle(const RecordMmapType* mmap_event);
    bool handle(const Reader::RecordCommType* comm);
    bool handle(const Reader::RecordSwitchCpuWideType* context_switch);
    bool handle(const Reader::RecordSwitchType* context_switch);

    void emplace_resolvers(Resolvers& resolvers);
    void end();

private:
    static constexpr int CCTX_LEVEL_PROCESS = 1;

    void update_calling_context(Process process, Thread thread, otf2::chrono::time_point tp,
                                bool switch_out);

    otf2::chrono::time_point adjust_timepoints(otf2::chrono::time_point tp);

    ExecutionScope scope_;

    trace::Trace& trace_;
    LocalCctxTree& local_cctx_tree_;

    otf2::event::metric cpuid_metric_event_;

    RawMemoryMapCache cached_mmap_events_;

    const time::Converter time_converter_;

    bool first_event_ = true;
    otf2::chrono::time_point first_time_point_;
    otf2::chrono::time_point last_time_point_;
};
} // namespace lo2s::perf::sample
