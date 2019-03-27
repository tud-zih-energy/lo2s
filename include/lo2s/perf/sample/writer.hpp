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
#include <lo2s/mmap.hpp>
#include <lo2s/perf/sample/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/definition/calling_context.hpp>
#include <otf2xx/definition/location.hpp>

#include <cstdint>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{
namespace monitor
{
class MainMonitor;
}

namespace perf
{
namespace sample
{

// Note, this cannot be protected for CRTP reasons...
class Writer : public Reader<Writer>
{
public:
    Writer(pid_t pid, pid_t tid, int cpu, monitor::MainMonitor& monitor, trace::Trace& trace,
           otf2::writer::local& otf2_writer, bool enable_on_exec);
    ~Writer();

public:
    using Reader<Writer>::handle;
    bool handle(const Reader::RecordSampleType* sample);
    bool handle(const Reader::RecordMmapType* mmap_event);
    bool handle(const Reader::RecordCommType* comm);
#ifdef USE_PERF_RECORD_SWITCH
    bool handle(const Reader::RecordSwitchCpuWideType* context_switch);
#endif

    otf2::writer::local& otf2_writer()
    {
        return otf2_writer_;
    }

    otf2::definition::location location() const
    {
        return otf2_writer_.location();
    }
    void end();

private:
    otf2::definition::calling_context::reference_type
    cctx_ref(const Reader::RecordSampleType* sample);
    trace::IpRefMap::iterator find_ip_child(Address addr, trace::IpRefMap& children);

    void update_current_thread(pid_t pid, pid_t tid, otf2::chrono::time_point tp);
    void leave_current_thread(pid_t pid, pid_t tid, otf2::chrono::time_point tp);

    pid_t pid_;
    pid_t tid_;
    int cpuid_;

    monitor::MainMonitor& monitor_;

    trace::Trace& trace_;
    otf2::writer::local& otf2_writer_;

    otf2::definition::metric_instance cpuid_metric_instance_;
    otf2::event::metric cpuid_metric_event_;

    trace::ThreadIpRefMap local_cctx_refs_;
    size_t next_cctx_ref_;
    trace::ThreadIpRefMap::value_type* current_thread_cctx_refs_ = nullptr;

    RawMemoryMapCache cached_mmap_events_;
    std::unordered_map<pid_t, std::string> comms_;

    const time::Converter time_converter_;

    bool first_event_ = true;
    otf2::chrono::time_point first_time_point_;
    otf2::chrono::time_point last_time_point_;
};
} // namespace sample
} // namespace perf
} // namespace lo2s
