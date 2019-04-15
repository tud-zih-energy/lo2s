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
#include <lo2s/process_info.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/otf2.hpp>

#include <cassert>
#include <cstring>

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

Writer::Writer(pid_t pid, pid_t tid, int cpu, monitor::MainMonitor& Monitor, trace::Trace& trace,
               otf2::writer::local& otf2_writer, bool enable_on_exec)
: Reader(tid, cpu, enable_on_exec), pid_(pid), tid_(tid), cpuid_(cpu), monitor_(Monitor),
  trace_(trace), otf2_writer_(otf2_writer),
  cpuid_metric_instance_(trace.metric_instance(trace.cpuid_metric_class(), otf2_writer.location(),
                                               otf2_writer.location())),
  cpuid_metric_event_(otf2::chrono::genesis(), cpuid_metric_instance_),
  thread_monitoring_cctx_refs_(
      tid, { pid, otf2::definition::calling_context::reference_type::undefined() }),
  time_converter_(perf::time::Converter::instance()), first_time_point_(lo2s::time::now()),
  last_time_point_(first_time_point_)
{
    // Must monitor either a CPU or (exclusive) a tid/pid
    assert((cpu == -1) ^ (pid == -1 && tid == -1));
    if (tid != -1)
    {
        // TODO create empty cctx root for thread monitoring
        thread_monitoring_cctx_refs_.second.pid = pid;
        current_thread_cctx_refs_ = &thread_monitoring_cctx_refs_;
    }
}

Writer::~Writer()
{

    if (current_thread_cctx_refs_ && tid_ == -1)
    {
        otf2_writer_.write_calling_context_leave(adjust_timepoints(lo2s::time::now()),
                                                 current_thread_cctx_refs_->second.entry.ref);
    }
    if (next_cctx_ref_ > 0)
    {
        const auto& mapping = trace_.merge_calling_contexts(local_cctx_refs_, next_cctx_ref_,
                                                            monitor_.get_process_infos());
        otf2_writer_ << mapping;
    }
}

trace::IpRefMap::iterator Writer::find_ip_child(Address addr, trace::IpRefMap& children)
{
    // -1 can't be inserted into the ip map, as it imples a 1-byte region from -1 to 0.
    if (addr == -1)
    {
        Log::debug() << "Got invalid ip (-1) from call stack. Replacing with -2.";
        addr = -2;
    }
    auto ret = children.emplace(std::piecewise_construct, std::forward_as_tuple(addr),
                                std::forward_as_tuple(next_cctx_ref_));
    if (ret.second)
    {
        next_cctx_ref_++;
    }
    return ret.first;
}
otf2::definition::calling_context::reference_type
Writer::cctx_ref(const Reader::RecordSampleType* sample)
{
    if (!has_cct_)
    {
        auto it = find_ip_child(sample->ip, current_thread_cctx_refs_->second.entry.children);
        return it->second.ref;
    }
    else
    {
        auto children = &current_thread_cctx_refs_->second.entry.children;
        for (uint64_t i = sample->nr - 1;; i--)
        {
            auto it = find_ip_child(sample->ips[i], *children);
            // We intentionally discard the last sample as it is somewhere in the kernel
            if (i == 1)
            {
                return it->second.ref;
            }

            children = &it->second.children;
        }
    }
}

bool Writer::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    tp = adjust_timepoints(tp);

    if (sample->pid == 0)
    {
        // WHY???
        Log::trace() << "sample from pid 0?";
        return false;
    }

    if (first_event_ && cpuid_ == -1)
    {
        first_time_point_ = std::min(first_time_point_, tp);
        otf2_writer_ << otf2::event::thread_begin(first_time_point_, trace_.process_comm(pid_), -1);
        first_event_ = false;
    }

    update_current_thread(sample->pid, sample->tid, tp);

    cpuid_metric_event_.timestamp(tp);
    cpuid_metric_event_.raw_values()[0] = sample->cpu;
    otf2_writer_ << cpuid_metric_event_;

    // we write the ugly raw ref-only events here due to performance reasons
    otf2_writer_.write_calling_context_sample(tp, cctx_ref(sample), 2,
                                              trace_.interrupt_generator().ref());

    return false;
}

bool Writer::handle(const Reader::RecordMmapType* mmap_event)
{
    // Since this is an mmap record (as opposed to mmap2), it will only be generated for executable
    if (cpuid_ == -1 && ((pid_t(mmap_event->pid) != pid_) || (pid_t(mmap_event->tid) != tid_)))
    {
        Log::warn() << "Inconsistent mmap pid/tid expected " << pid_ << "/" << tid_ << ", actual "
                    << mmap_event->pid << "/" << mmap_event->tid;
    }
    Log::debug() << "encountered mmap event for " << pid_ << "," << tid_ << " "
                 << Address(mmap_event->addr) << " len: " << Address(mmap_event->len)
                 << " pgoff: " << Address(mmap_event->pgoff) << ", " << mmap_event->filename;

    cached_mmap_events_.emplace_back(mmap_event);
    return false;
}

void Writer::update_current_thread(pid_t pid, pid_t tid, otf2::chrono::time_point tp)
{

    if (tid_ != -1)
    {
        if (tid != tid_ || pid != pid_)
        {
            Log::warn() << "Sample outside of monitored process";
        }
        assert(tid == tid_ && pid == pid_);

        auto ret = local_cctx_refs_.emplace(std::piecewise_construct, std::forward_as_tuple(tid),
                                            std::forward_as_tuple(pid, next_cctx_ref_));

        if (ret.second)
        {
            next_cctx_ref_++;
        }

        current_thread_cctx_refs_ = &(*ret.first);
        return;
    }
    tp = adjust_timepoints(tp);

    // CPU monitoring
    if (current_thread_cctx_refs_ && current_thread_cctx_refs_->first == tid)
    {
        // current thread is unchanged
        return;
    }
    else if (current_thread_cctx_refs_)
    {

        // need to leave
        otf2_writer_.write_calling_context_leave(tp, current_thread_cctx_refs_->second.entry.ref);
    }
    // thread has changed
    auto ret = local_cctx_refs_.emplace(std::piecewise_construct, std::forward_as_tuple(tid),
                                        std::forward_as_tuple(pid, next_cctx_ref_));
    if (ret.second)
    {
        next_cctx_ref_++;
    }

    otf2_writer_.write_calling_context_enter(tp, ret.first->second.entry.ref, 2);
    current_thread_cctx_refs_ = &(*ret.first);
}

void Writer::leave_current_thread(pid_t tid, otf2::chrono::time_point tp)
{
    if (tid_ != -1)
    {
        // should not happen (TM)
        assert(false);
        return;
    }

    if (current_thread_cctx_refs_ == nullptr)
    {
        // Already not in a thread
        return;
    }

    if (current_thread_cctx_refs_->first != tid)
    {
        Log::debug() << "inconsistent leave thread"; // will probably set to trace sooner or later
    }
    tp = adjust_timepoints(tp);

    otf2_writer_.write_calling_context_leave(tp, current_thread_cctx_refs_->second.entry.ref);
    current_thread_cctx_refs_ = nullptr;
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

#ifdef USE_PERF_RECORD_SWITCH
bool Writer::handle(const Reader::RecordSwitchCpuWideType* context_switch)
{
    assert(cpuid_ != -1);
    auto tp = time_converter_(context_switch->time);

    if (context_switch->header.misc & PERF_RECORD_MISC_SWITCH_OUT)
    {
        leave_current_thread(context_switch->tid, tp);
    }
    else
    {
        update_current_thread(context_switch->pid, context_switch->tid, tp);
    }

    return false;
}
#endif

bool Writer::handle(const Reader::RecordCommType* comm)
{
    if (cpuid_ == -1)
    {
        std::string new_command{ static_cast<const char*>(comm->comm) };

        Log::debug() << "Thread " << comm->tid << " in process " << comm->pid
                     << " changed name to \"" << new_command << "\"";

        // update task name
        trace_.update_thread_name(comm->tid, new_command);

        // only update name of process if the main thread changes its name
        if (comm->pid == comm->tid)
        {
            trace_.update_process_name(comm->pid, new_command);
        }
    }
    else
    {
        summary().register_process(comm->pid);

        comms_[comm->pid] = comm->comm;
    }

    return false;
}

void Writer::end()
{
    if (cpuid_ == -1)
    {
        last_time_point_ = adjust_timepoints(lo2s::time::now());

        // If we have never written any samples on this location, we also never
        // got to write the thread_begin event.  Make sure we do that now.
        if (first_event_)
        {
            // first_time_point_ will always be before any other timestamp on
            // this location.  If no samples were written, it is initialized by
            // time::now(), which is a monotone clock, therefore it is before
            // the call to time::now() from above.  If any samples were written,
            // the required check has occured in handle() above.
            otf2_writer_ << otf2::event::thread_begin(first_time_point_, trace_.process_comm(pid_),
                                                      -1);
        }

        // At this point, transitivity and monotonicity (of lo2s::time::now())
        // ensure that first_time_point_ <= last_time_point_, therefore samples
        // on this location span a non-negative amount of time between the
        // thread_begin and thread_end event.
        otf2_writer_ << otf2::event::thread_end(last_time_point_, trace_.process_comm(pid_), -1);
    }

    trace_.add_threads(comms_);

    monitor_.insert_cached_mmap_events(cached_mmap_events_);
}
} // namespace sample
} // namespace perf
} // namespace lo2s
