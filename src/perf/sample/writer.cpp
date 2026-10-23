// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/perf/sample/writer.hpp>

#include <lo2s/address.hpp>
#include <lo2s/calling_context.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/sample/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/types.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/reg_keys.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/event/thread_begin.hpp>
#include <otf2xx/event/thread_end.hpp>
#include <otf2xx/exception.hpp>

#include <exception>
#include <map>
#include <optional>
#include <string>
#include <variant>

#include <cassert>
#include <cstdint>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf::sample
{

Writer::Writer(std::variant<Cpu, Thread> scope, trace::Trace& trace, bool enable_on_exec)
: Reader(scope, enable_on_exec), scope_(scope), trace_(trace),
  local_cctx_tree_(trace.create_local_cctx_tree(trace::SampleKind::SAMPLE, scope)),
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
            Mapping const m(Address(mmap_event.get()->addr),
                            Address(mmap_event.get()->addr + mmap_event.get()->len),
                            Address(mmap_event.get()->pgoff));
            Process p(mmap_event.get()->pid);

            const auto fr = function_resolver_for(mmap_event.get()->filename);

            resolvers.function_resolvers.emplace(p, p);
            if (fr != nullptr)
            {
                resolvers.function_resolvers.at(p).emplace(m, fr);
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
    if (std::holds_alternative<Thread>(scope_) &&
        std::get<Thread>(scope_) != Thread(mmap_event->tid))
    {
        Log::warn() << "Inconsistent mmap expected " << scope_get_name(scope_) << ", actual "
                    << mmap_event->tid;
    }

    Log::trace() << "encountered mmap event for process " << Process(mmap_event->pid) << ": ";
    Log::trace() << Mapping(Address(mmap_event->addr), Address(mmap_event->addr + mmap_event->len),
                            Address(mmap_event->pgoff))
                 << " " << mmap_event->filename;

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
    assert(std::holds_alternative<Cpu>(scope_));
    auto tp = time_converter_(context_switch->time);
    tp = adjust_timepoints(tp);

    update_calling_context(Process(context_switch->pid), Thread(context_switch->tid), tp,
                           context_switch->header.misc & PERF_RECORD_MISC_SWITCH_OUT);

    return false;
}

bool Writer::handle(const Reader::RecordSwitchType* context_switch)
{
    assert(!std::holds_alternative<Cpu>(scope_));
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
        if (first_event_ && std::holds_alternative<Thread>(scope_))
        {
            local_cctx_tree_.writer() << otf2::event::thread_begin(
                tp,
                trace_
                    .process_handle(Process::no_parent(),
                                    ThreadGroup::instance().get_process(thread), std::nullopt)
                    .comm(),
                -1);
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
        trace_.process_handle(Process::no_parent(), Process(comm->pid), new_command);
    }
    else
    {
        trace_.thread_handle(Process(comm->pid), Thread(comm->tid), new_command);
    }
    summary().register_process(Process(comm->pid));

    return false;
}

void Writer::end()
{
    if (std::holds_alternative<Thread>(scope_))
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
                first_time_point_,
                trace_
                    .process_handle(Process::no_parent(),
                                    ThreadGroup::instance().get_process(std::get<Thread>(scope_)),
                                    std::nullopt)
                    .comm(),
                -1);
        }

        // At this point, transitivity and monotonicity (of lo2s::time::now())
        // ensure that first_time_point_ <= last_time_point_, therefore samples
        // on this scope span a non-negative amount of time between the
        // thread_begin and thread_end event.
        local_cctx_tree_.writer() << otf2::event::thread_end(
            last_time_point_,
            trace_
                .process_handle(Process::no_parent(),
                                ThreadGroup::instance().get_process(std::get<Thread>(scope_)),
                                std::nullopt)
                .comm(),
            -1);
    }
}
} // namespace lo2s::perf::sample
