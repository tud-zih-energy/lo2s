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

#include <lo2s/perf/sample/reader.hpp>

#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/time/converter.hpp>

#include <lo2s/address.hpp>
#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/thread_monitor.hpp>
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
: Reader(config().enable_cct), pid_(pid), tid_(tid), cpuid_(cpu), monitor_(Monitor), trace_(trace),
  otf2_writer_(otf2_writer),
  cpuid_metric_instance_(trace.metric_instance(trace.cpuid_metric_class(), otf2_writer.location(),
                                               otf2_writer.location())),
  cpuid_metric_event_(otf2::chrono::genesis(), cpuid_metric_instance_),
  time_converter_(perf::time::Converter::instance()), first_time_point_(lo2s::time::now())
{

    CounterDescription sampling_event =
        EventProvider::get_event_by_name(config().sampling_event); // config parser has already
                                                                   // checked for event
                                                                   // availability, should not throw

    Log::debug() << "using sampling event \'" << config().sampling_event
                 << "\', period: " << config().sampling_period;

    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.size = sizeof(struct perf_event_attr);
    attr.type = sampling_event.type;
    attr.config = sampling_event.config;
    attr.config1 = sampling_event.config1;
    attr.sample_period = config().sampling_period;
    attr.exclude_kernel = config().exclude_kernel;

    // map events to buffer (don't need the fancy mmap2)
    attr.mmap = 1;

    init(attr, tid, cpu, enable_on_exec, config().mmap_pages);
}

Writer::~Writer()
{
    // TODO FIXME
    // Actually we have to merge the maps_ within one process before this step :-(
    if (!local_ip_refs_.empty())
    {
        const auto& mapping = trace_.merge_ips(local_ip_refs_, monitor_.get_process_infos());
        otf2_writer_ << mapping;
    }
}

trace::IpRefMap::iterator Writer::find_ip_child(Address addr, pid_t pid, trace::IpRefMap& children)
{
    // -1 can't be inserted into the ip map, as it imples a 1-byte region from -1 to 0.
    if (addr == -1)
    {
        Log::debug() << "Got invalid ip (-1) from call stack. Replacing with -2.";
        addr = -2;
    }
    auto ret = children.emplace(std::piecewise_construct, std::forward_as_tuple(addr),
                                std::forward_as_tuple(pid, next_ip_ref()));
    return ret.first;
}
otf2::definition::calling_context::reference_type
Writer::cctx_ref(const Reader::RecordSampleType* sample)
{
    if (!has_cct_)
    {
        auto it = find_ip_child(sample->ip, sample->pid, local_ip_refs_);
        return it->second.ref;
    }
    else
    {
        auto children = &local_ip_refs_;
        for (uint64_t i = sample->nr - 1;; i--)
        {
            auto it = find_ip_child(sample->ips[i], sample->pid, *children);
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
    if (sample->pid == 0)
    {
        return false;
    }
    //    log::trace() << "sample event. ip: " << sample->ip << ", time: " << sample->time
    //                 << ", pid: " << sample->pid << ", tid: " << sample->tid;
    auto tp = time_converter_(sample->time);
    /*
    Note: This is the inefficient version with a fake calling context that we don't use
    otf2::definition::calling_context cctx(cctx_ref(sample),
                                            otf2::definition::region::undefined(),
                                           otf2::definition::source_code_location::undefined(),
                                           otf2::definition::calling_context::undefined());
    otf2::event::calling_context_sample ccs(tp, cctx, 2, trace_.interrupt_generator());
    otf2_writer_ << ccs;
    */
    if (first_event_ && cpuid_ == -1)
    {
        first_time_point_ = std::min(first_time_point_, tp);
        otf2_writer_ << otf2::event::thread_begin(first_time_point_, trace_.process_comm(pid_), -1);
        first_event_ = false;
    }

    cpuid_metric_event_.timestamp(tp);
    cpuid_metric_event_.raw_values()[0] = sample->cpu;
    otf2_writer_ << cpuid_metric_event_;

    otf2_writer_.write_calling_context_sample(tp, cctx_ref(sample), 2,
                                              trace_.interrupt_generator().ref());

    last_time_point_ = tp;
    return false;
}

bool Writer::handle(const Reader::RecordMmapType* mmap_event)
{
    if (cpuid_ == -1 && ((pid_t(mmap_event->pid) != pid_) || (pid_t(mmap_event->tid) != tid_)))
    {
        Log::warn() << "Inconsistent mmap pid/tid expected " << pid_ << "/" << tid_ << ", actual "
                    << mmap_event->pid << "/" << mmap_event->tid;
    }
    Log::debug() << "encountered mmap event for " << pid_ << "," << tid_ << " "
                 << Address(mmap_event->addr) << " len: " << Address(mmap_event->len)
                 << " pgoff: " << Address(mmap_event->pgoff) << ", " << mmap_event->filename;

    cached_mmap_events.emplace_back(*mmap_event);
    return false;
}

bool Writer::handle(const Reader::RecordCommType* comm)
{
    std::string new_command{ static_cast<const char*>(comm->comm) };

    // update task name
    trace_.task_update_command(comm->tid, new_command);

    // only update name of process if the main thread changes its name
    if (comm->pid == comm->tid)
    {
        trace_.process_update_executable(comm->pid, new_command);
    }

    return false;
}

void Writer::end()
{
    if (cpuid_ == -1)
    {
        // time::now() can sometimes can be in the past :-(
        last_time_point_ = std::max(last_time_point_, lo2s::time::now());
        otf2_writer_ << otf2::event::thread_end(last_time_point_, trace_.process_comm(pid_), -1);
    }
    monitor_.insert_cached_mmap_events(cached_mmap_events);
}
} // namespace sample
} // namespace perf
} // namespace lo2s
