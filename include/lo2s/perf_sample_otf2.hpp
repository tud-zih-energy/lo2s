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
#include <lo2s/monitor_config.hpp>
#include <lo2s/otf2_trace.hpp>
#include <lo2s/perf_sample_reader.hpp>
#include <lo2s/time/converter.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/definition/calling_context.hpp>
#include <otf2xx/definition/location.hpp>

#include <cstdint>

extern "C" {
#include <sys/types.h>
}

namespace lo2s
{
class thread_monitor;

class perf_sample_otf2 : public perf_sample_reader<perf_sample_otf2>
{
public:
    perf_sample_otf2(pid_t pid, pid_t tid, const monitor_config& config, thread_monitor& monitor,
                     otf2_trace& trace, const perf_time_converter& time_converter,
                     bool enable_on_exec);
    ~perf_sample_otf2();

public:
    using perf_sample_reader<perf_sample_otf2>::handle;
    bool handle(const perf_sample_reader::record_sample_type* sample);
    bool handle(const perf_sample_reader::record_mmap_type* mmap_event);

    otf2::writer::local& writer()
    {
        return writer_;
    }

    otf2::definition::location location() const
    {
        return writer_.location();
    }
    void end();

private:
    otf2::definition::calling_context::reference_type
    cctx_ref(const perf_sample_reader::record_sample_type* sample);
    ip_ref_map::iterator find_ip_child(address addr, ip_ref_map& children);

    pid_t pid_;
    pid_t tid_;
    monitor_config config_;
    thread_monitor& monitor_;

    otf2_trace& trace_;
    otf2::writer::local& writer_;

    ip_ref_map local_ip_refs_;
    std::uint64_t ip_ref_counter_ = 0;
    perf_time_converter time_converter_;

    bool first_event_ = true;
    otf2::chrono::time_point last_time_point_ = otf2::chrono::genesis();
};
}
