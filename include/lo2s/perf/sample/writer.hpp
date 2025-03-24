/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/local_cctx_tree.hpp>

#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/perf/sample/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/definition/calling_context.hpp>
#include <otf2xx/definition/location.hpp>

#include <cstdint>
#include <unordered_map>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{
namespace perf
{
namespace sample
{

// Note, this cannot be protected for CRTP reasons...
class Writer : public Reader<Writer>
{
public:
    Writer(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec);
    ~Writer();

public:
    using Reader<Writer>::handle;
    bool handle(const Reader::RecordSampleType* sample);
    bool handle(const Reader::RecordMmapType* mmap_event);
    bool handle(const Reader::RecordCommType* comm);
    bool handle(const Reader::RecordSwitchCpuWideType* context_switch);
    bool handle(const Reader::RecordSwitchType* context_switch);

    void emplace_resolvers(Resolvers& resolvers);
    void end();

private:
    void enter_calling_context(Process process, Thread thread, otf2::chrono::time_point tp);
    void update_calling_context(Process process, Thread thread, otf2::chrono::time_point tp,
                                bool switch_out);

    void leave_current_process();
    void leave_current_thread(otf2::chrono::time_point tp);

    otf2::chrono::time_point adjust_timepoints(otf2::chrono::time_point tp);

    ExecutionScope scope_;

    trace::Trace& trace_;

    otf2::definition::metric_instance cpuid_metric_instance_;
    otf2::event::metric cpuid_metric_event_;

    LocalCctxTree& local_cctx_tree_;

    RawMemoryMapCache cached_mmap_events_;

    const time::Converter time_converter_;

    bool first_event_ = true;
    otf2::chrono::time_point first_time_point_;
    otf2::chrono::time_point last_time_point_;
};
} // namespace sample
} // namespace perf
} // namespace lo2s
