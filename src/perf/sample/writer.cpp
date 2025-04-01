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

#include <lo2s/summary.hpp>

#include <lo2s/perf/sample/writer.hpp>

#include <lo2s/perf/sample/reader.hpp>

#include <lo2s/perf/time/converter.hpp>

#include <lo2s/address.hpp>
#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/otf2.hpp>

#include <cassert>
#include <cstring>
#include <map>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{
namespace perf
{
namespace sample
{

Writer::Writer(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec)
: Reader(scope, enable_on_exec), scope_(scope), trace_(trace),
  otf2_writer_(trace.sample_writer(scope)),
  cpuid_metric_instance_(trace.metric_instance(trace.cpuid_metric_class(), otf2_writer_.location(),
                                               otf2_writer_.location())),
  cpuid_metric_event_(otf2::chrono::genesis(), cpuid_metric_instance_),
  local_cctx_tree_(trace.create_local_cctx_tree()),
  time_converter_(perf::time::Converter::instance()), first_time_point_(lo2s::time::now()),
  last_time_point_(first_time_point_)
{
}

Writer::~Writer()
{
    if (local_cctx_tree_.cur_cctx().type == CallingContextType::THREAD)
    {
        otf2_writer_.write_calling_context_leave(adjust_timepoints(lo2s::time::now()),
                                                 local_cctx_tree_.cctx_exit());
    }

    local_cctx_tree_.finalize(&otf2_writer_);
}

void Writer::emplace_resolvers(Resolvers& resolvers)
{

    for (auto& mmap_event : cached_mmap_events_)
    {
        try
        {
            Mapping m(mmap_event.get()->addr, mmap_event.get()->addr + mmap_event.get()->len,
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

    update_current_thread(Process(sample->pid), Thread(sample->tid), tp);

    if (!has_cct_)
    {
        // For unwind distance definiton, see:
        // http://scorepci.pages.jsc.fz-juelich.de/otf2-pipelines/docs/otf2-2.2/html/group__records__definition.
        // html#CallingContext

        // We always have a fake CallingContext for the process, which is scheduled.
        // So if we don't record the calling context tree, all nodes in the calling context tree
        // are: The fake node for the process and a second node for the current context of the
        // sample. Therefore, we always have an unwind distance of 2. (Technically, it could also be
        // 1, but we lack the information to distinguish those cases. Hence, we stick with 2.)
        //
        // However, in the case that we actually record the complete call stack, we get the number
        // of nodes in the calling context tree from the number of elements in the call stack
        // recorded from perf. Tough, we still have the fake calling context for the process, which
        // adds 1 to that number. But we also remove the lowest entry from the call stack, because
        // that is ALWAYS in the kernel, which can't be resolved and thus doesn't provide any useful
        // information.
        //
        // Having these things in mind, look at this line and tell me, why it is still wrong:
        otf2_writer_.write_calling_context_sample(tp, local_cctx_tree_.sample_ref(sample->ip), 2,
                                                  trace_.interrupt_generator().ref());
    }
    else
    {
        otf2_writer_.write_calling_context_sample(
            tp, local_cctx_tree_.sample_ref(sample->nr, sample->ips), sample->nr,
            trace_.interrupt_generator().ref());
    }
    return false;
}

bool Writer::handle(const Reader::RecordMmapType* mmap_event)
{
    // Since this is an mmap record (as opposed to mmap2), it will only be generated for executable
    if (!scope_.is_cpu() && scope_ != ExecutionScope(Thread(mmap_event->tid)))
    {
        Log::warn() << "Inconsistent mmap expected " << scope_.name() << ", actual "
                    << mmap_event->tid;
    }

    Log::debug() << "encountered mmap event for " << scope_.name() << " "
                 << Address(mmap_event->addr) << " len: " << Address(mmap_event->len)
                 << " pgoff: " << Address(mmap_event->pgoff) << ", " << mmap_event->filename;

    cached_mmap_events_.emplace_back(mmap_event);

    return false;
}

void Writer::update_current_thread(Process process, Thread thread, otf2::chrono::time_point tp)
{
    if (first_event_ && !scope_.is_cpu())
    {
        otf2_writer_ << otf2::event::thread_begin(tp, trace_.process_comm(thread), -1);
        first_event_ = false;
    }

    if (local_cctx_tree_.is_current(CallingContext::thread(thread)))
    {
        return;
    }
    else if (local_cctx_tree_.cur_cctx().type != CallingContextType::PROCESS)
    {
        leave_current_thread(tp);
    }

    update_current_process(process);

    otf2_writer_.write_calling_context_enter(
        tp, local_cctx_tree_.cctx_enter(CallingContext::thread(thread)), 2);
}

void Writer::leave_current_thread(otf2::chrono::time_point tp)
{
    if (local_cctx_tree_.cur_cctx().type == CallingContextType::THREAD)
    {
        otf2_writer_.write_calling_context_leave(tp, local_cctx_tree_.cctx_exit());
    }
}

void Writer::update_current_process(Process process)
{
    if (local_cctx_tree_.is_current(CallingContext::process(process)))
    {
        return;
    }

    if (local_cctx_tree_.cur_cctx().type != CallingContextType::ROOT)
    {
        leave_current_process();
    }

    local_cctx_tree_.cctx_enter(CallingContext::process(process));
}

void Writer::leave_current_process()
{
    if (local_cctx_tree_.cur_cctx().type == CallingContextType::PROCESS)
    {
        local_cctx_tree_.cctx_exit();
    }
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

    bool is_switch_out = context_switch->header.misc & PERF_RECORD_MISC_SWITCH_OUT;

    update_calling_context(Process(context_switch->pid), Thread(context_switch->tid), tp,
                           is_switch_out);

    cpuid_metric_event_.timestamp(tp);
    cpuid_metric_event_.raw_values()[0] =
        is_switch_out ? -1 : static_cast<std::int64_t>(context_switch->cpu);

    otf2_writer_ << cpuid_metric_event_;

    return false;
}

void Writer::update_calling_context(Process process, Thread thread, otf2::chrono::time_point tp,
                                    bool switch_out)
{
    if (switch_out)
    {
        if (local_cctx_tree_.cur_cctx().type == CallingContextType::ROOT)
        {
            Log::debug() << "Leave event but not in a thread!";
            return;
        }
        leave_current_thread(tp);
    }
    else
    {
        update_current_thread(process, thread, tp);
    }
}

bool Writer::handle(const Reader::RecordCommType* comm)
{
    if (!scope_.is_cpu())
    {
        std::string new_command{ static_cast<const char*>(comm->comm) };

        Log::debug() << "Thread " << comm->tid << " in process " << comm->pid
                     << " changed name to \"" << new_command << "\"";

        // update task name
        trace_.update_thread_name(Thread(comm->tid), new_command);

        // only update name of process if the main thread changes its name
        if (comm->pid == comm->tid)
        {
            trace_.update_process_name(Process(comm->pid), new_command);
        }
        else
        {
            trace_.update_thread_name(Thread(comm->tid), new_command);
        }
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
            otf2_writer_ << otf2::event::thread_begin(first_time_point_,
                                                      trace_.process_comm(scope_.as_thread()), -1);
        }

        // At this point, transitivity and monotonicity (of lo2s::time::now())
        // ensure that first_time_point_ <= last_time_point_, therefore samples
        // on this scope span a non-negative amount of time between the
        // thread_begin and thread_end event.
        otf2_writer_ << otf2::event::thread_end(last_time_point_,
                                                trace_.process_comm(scope_.as_thread()), -1);
    }
}
} // namespace sample
} // namespace perf
} // namespace lo2s
