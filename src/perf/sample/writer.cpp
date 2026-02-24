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

#include <lo2s/perf/sample/writer.hpp>

#include <lo2s/address.hpp>
#include <lo2s/calling_context.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/instruction_resolver.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/sample/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/exception.hpp>

#include <exception>
#include <map>

#include <cassert>
#include <cstdint>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf::sample
{

Writer::Writer(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec)
: Reader(scope, enable_on_exec), scope_(scope), trace_(trace),
  local_cctx_tree_(trace.create_local_cctx_tree(MeasurementScope::sample(scope))),
  cpuid_metric_event_(otf2::chrono::genesis(),
                      trace.metric_instance(trace.cpuid_metric_class(),
                                            local_cctx_tree_.writer().location(),
                                            local_cctx_tree_.writer().location())),
  time_converter_(perf::time::Converter::instance()), first_time_point_(lo2s::time::now()),
  last_time_point_(first_time_point_)
{
}

Writer::~Writer()
{
    try
    {
        local_cctx_tree_.cctx_leave(last_time_point_, CCTX_LEVEL_PROCESS);
    }
    catch (otf2::exception& e)
    {
        Log::error() << "Could not write final sample leave, your trace might be broken!";
    }
    local_cctx_tree_.finalize();
}

void Writer::emplace_resolvers(Resolvers& resolvers)
{

    for (auto& mmap_event : cached_mmap_events_)
    {
        try
        {
            Mapping const m(mmap_event.get()->addr, mmap_event.get()->addr + mmap_event.get()->len,
                            mmap_event.get()->pgoff);
            Process p(mmap_event.get()->pid);

            auto fr = function_resolver_for(mmap_event.get()->filename);

            resolvers.function_resolvers.emplace(p, p);
            if (fr != nullptr)
            {
                resolvers.function_resolvers.at(p).emplace(m, fr);
            }
            auto ir = instruction_resolver_for(mmap_event.get()->filename);
            if (ir != nullptr)
            {
                resolvers.instruction_resolvers[p].emplace(m, ir);
            }
        }
        catch (std::exception& e)
        {
            Log::error() << "Could not emplace resolvers for " << mmap_event.get()->filename << ": "
                         << e.what();
        }
    }
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    tp = adjust_timepoints(tp);

    update_calling_context(Process(sample->pid), Thread(sample->tid), tp, false);

    if (!record_callgraph_)
    {
        local_cctx_tree_.cctx_sample(tp, sample->ip);
    }
    else
    {
        local_cctx_tree_.cctx_sample(tp, sample->nr, sample->ips);
    }
    return false;
}

bool Writer::handle(const RecordMmapType* mmap_event)
{
    // Since this is an mmap record (as opposed to mmap2), it will only be generated for executable
    if (!scope_.is_cpu() && scope_ != ExecutionScope(Thread(mmap_event->tid)))
    {
        Log::warn() << "Inconsistent mmap expected " << scope_.name() << ", actual "
                    << mmap_event->tid;
    }

    Log::debug() << "encountered mmap event for " << mmap_event->pid << " "
                 << Address(mmap_event->addr) << " len: " << Address(mmap_event->len)
                 << " pgoff: " << Address(mmap_event->pgoff) << ", " << mmap_event->filename;

    cached_mmap_events_.emplace_back(mmap_event);

    return false;
}

otf2::chrono::time_point Writer::adjust_timepoints(otf2::chrono::time_point tp)
{
    // as the perf timepoints can not be trusted to be in order all the time fix them here
    if (last_time_point_ > tp)
    {
        Log::debug() << "perf_event_open timestamps not in order: " << last_time_point_ << ">"
                     << tp;

        tp = last_time_point_;
    }

    last_time_point_ = tp;
    return tp;
}

bool Writer::handle(const Reader::RecordSwitchCpuWideType* context_switch)
{
    assert(scope_.is_cpu());
    auto tp = time_converter_(context_switch->time);
    tp = adjust_timepoints(tp);

    update_calling_context(Process(context_switch->pid), Thread(context_switch->tid), tp,
                           context_switch->header.misc & PERF_RECORD_MISC_SWITCH_OUT);

    return false;
}

bool Writer::handle(const Reader::RecordSwitchType* context_switch)
{
    assert(!scope_.is_cpu());
    auto tp = time_converter_(context_switch->time);
    tp = adjust_timepoints(tp);

    bool const is_switch_out = context_switch->header.misc & PERF_RECORD_MISC_SWITCH_OUT;

    update_calling_context(Process(context_switch->pid), Thread(context_switch->tid), tp,
                           is_switch_out);

    cpuid_metric_event_.timestamp(tp);
    cpuid_metric_event_.raw_values()[0] =
        is_switch_out ? -1 : static_cast<std::int64_t>(context_switch->cpu);

    local_cctx_tree_.writer() << cpuid_metric_event_;

    return false;
}

void Writer::update_calling_context(Process process, Thread thread, otf2::chrono::time_point tp,
                                    bool switch_out)
{
    if (switch_out)
    {
        local_cctx_tree_.cctx_leave(tp, CCTX_LEVEL_PROCESS);
    }
    else
    {
        if (first_event_ && !scope_.is_cpu())
        {
            local_cctx_tree_.writer()
                << otf2::event::thread_begin(tp, trace_.process_comm(thread), -1);
            first_event_ = false;
        }

        local_cctx_tree_.cctx_enter(tp, CCTX_LEVEL_PROCESS, CallingContext::process(process),
                                    CallingContext::thread(thread));
    }
}

bool Writer::handle(const Reader::RecordCommType* comm)
{
    std::string const new_command{ static_cast<const char*>(comm->comm) };

    Log::debug() << "Thread " << comm->tid << " in process " << comm->pid << " changed name to \""
                 << new_command << "\"";

    // only update name of process if the main thread changes its name
    if (comm->pid == comm->tid)
    {
        trace_.emplace_process(Process::no_parent(), Process(comm->pid), new_command);
    }
    else
    {
        trace_.emplace_thread(Process(comm->pid), Thread(comm->tid), new_command);
    }
    summary().register_process(Process(comm->pid));

    return false;
}

void Writer::end()
{
    if (!scope_.is_cpu())
    {
        adjust_timepoints(lo2s::time::now());
        // If we have never written any samples on this scope, we also never
        // got to write the thread_begin event.  Make sure we do that now.
        if (first_event_)
        {
            // first_time_point_ will always be before any other timestamp on
            // this scope.  If no samples were written, it is initialized by
            // time::now(), which is a monotone clock, therefore it is before
            // the call to time::now() from above.  If any samples were written,
            // the required check has occured in handle() above.
            local_cctx_tree_.writer() << otf2::event::thread_begin(
                first_time_point_, trace_.process_comm(scope_.as_thread()), -1);
        }

        // At this point, transitivity and monotonicity (of lo2s::time::now())
        // ensure that first_time_point_ <= last_time_point_, therefore samples
        // on this scope span a non-negative amount of time between the
        // thread_begin and thread_end event.
        local_cctx_tree_.writer() << otf2::event::thread_end(
            last_time_point_, trace_.process_comm(scope_.as_thread()), -1);
    }
}
} // namespace lo2s::perf::sample
