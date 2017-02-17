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

#include <lo2s/perf_sample_otf2.hpp>

#include <lo2s/address.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/otf2_trace.hpp>
#include <lo2s/perf_sample_reader.hpp>
#include <lo2s/process_info.hpp>
#include <lo2s/thread_monitor.hpp>
#include <lo2s/time/converter.hpp>

#include <otf2xx/otf2.hpp>

#include <cassert>
#include <cstring>

extern "C" {
#include <linux/perf_event.h>
}

namespace lo2s
{
perf_sample_otf2::perf_sample_otf2(pid_t pid, pid_t tid, const monitor_config& config,
                                   thread_monitor& monitor, otf2_trace& trace,
                                   const perf_time_converter& time_converter, bool enable_on_exec)
: perf_sample_reader(config.enable_cct), pid_(pid), tid_(tid), config_(config), monitor_(monitor),
  trace_(trace), writer_(trace.sample_writer(pid, tid)), time_converter_(time_converter)
{

    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.size = sizeof(struct perf_event_attr);
    attr.type = PERF_TYPE_HARDWARE;
    attr.config = PERF_COUNT_HW_INSTRUCTIONS;
    attr.sample_period = config_.sampling_period;

    // map events to buffer
    attr.mmap = 1;
    // Note: we do not need the fancy format!
    // attr.mmap2 = 1;

    // not used yet
    // attr.task = 1;

    init(attr, tid, enable_on_exec, config.mmap_pages);
}

perf_sample_otf2::~perf_sample_otf2()
{
    // TODO FIXME
    // Actually we have to merge the maps_ within one process before this step :-(
    if (!local_ip_refs_.empty())
    {
        const auto& mapping =
            trace_.merge_ips(local_ip_refs_, ip_ref_counter_, monitor_.info().maps());
        writer_ << mapping;
    }
}

ip_ref_map::iterator perf_sample_otf2::find_ip_child(address addr, ip_ref_map& children)
{
    // -1 can't be inserted into the ip map, as it imples a 1-byte region from -1 to 0.
    if (addr == -1)
    {
        log::debug() << "Got invalid ip (-1) from call stack. Replacing with -2.";
        addr = -2;
    }
    auto it = children.find(addr);
    if (it == children.end())
    {
        otf2::definition::calling_context::reference_type ref(ip_ref_counter_++);
        auto ret = children.emplace(addr, ref);
        assert(ret.second);
        it = ret.first;
    }
    return it;
}

otf2::definition::calling_context::reference_type
perf_sample_otf2::cctx_ref(const perf_sample_reader::record_sample_type* sample)
{
    if (!has_cct_)
    {
        auto it = find_ip_child(sample->ip, local_ip_refs_);
        return it->second.ref;
    }
    else
    {
        auto children = &local_ip_refs_;
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

bool perf_sample_otf2::handle(const perf_sample_reader::record_sample_type* sample)
{
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
    writer_ << ccs;
    */
    if (first_event_)
    {
        first_event_ = false;
        writer_ << otf2::event::thread_begin(tp - otf2::chrono::time_point::duration(1),
                                             trace_.self_comm(), -1);
        // TODO: figure out what we actually need to write here to be a correct OTF2 trace...
        // writer_ << otf2::event::thread_team_begin(tp - otf2::chrono::time_point::duration(1),
        //                                          trace_.self_comm());
    }

    writer_.write_calling_context_sample(tp, cctx_ref(sample), 2,
                                         trace_.interrupt_generator().ref());
    last_time_point_ = tp;
    return false;
}

bool perf_sample_otf2::handle(const perf_sample_reader::record_mmap_type* mmap_event)
{
    if ((pid_t(mmap_event->pid) != pid_) || (pid_t(mmap_event->tid) != tid_))
    {
        log::warn() << "Inconsistent mmap pid/tid expected " << pid_ << "/" << tid_ << ", actual "
                    << mmap_event->pid << "/" << mmap_event->tid;
    }
    log::debug() << "encountered mmap event for " << pid_ << "," << tid_ << " "
                 << address(mmap_event->addr) << " len: " << address(mmap_event->len)
                 << " pgoff: " << address(mmap_event->pgoff) << ", " << mmap_event->filename;
    // Firefox does that... no idea what it is
    monitor_.info().mmap(mmap_event->addr, mmap_event->addr + mmap_event->len, mmap_event->pgoff,
                         mmap_event->filename);
    return false;
}

void perf_sample_otf2::end()
{
    // get_time() sometims can be in the past :-(
    writer_ << otf2::event::thread_end(last_time_point_, trace_.self_comm(), -1);
    // TODO, or thread_team_end?
}
}
