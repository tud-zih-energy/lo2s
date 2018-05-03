/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

#include <lo2s/dtrace/event_reader.hpp>
#include <lo2s/dtrace/time/convert.hpp>

#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/detail/weak_ref.hpp>
#include <otf2xx/definition/region.hpp>

#include <sstream>
#include <unordered_map>

namespace lo2s
{
namespace dtrace
{
namespace sched
{
class Writer : public EventReader
{
    struct data
    {
        int64_t ts;
        int pid;
        int tid;
        int prio;
        int i = 0;
    };

public:
    Writer(int cpu, trace::Trace& trace);
    ~Writer();

    void init(pid_t tid, int cpu);

    void handle(int cpu, const ProbeDesc& pd, nitro::lang::string_ref data,
                void* raw_data) override;

    otf2::definition::region::reference_type thread_region_ref(pid_t tid);

    void merge_trace();

private:
    otf2::writer::local& otf2_writer_;
    trace::Trace& trace_;
    const time::Converter time_converter_;

    using region_ref = otf2::definition::region::reference_type;
    std::unordered_map<pid_t, region_ref> thread_region_refs_;
    std::unordered_map<pid_t, std::string> comms_;

    region_ref current_region_ = region_ref::undefined();

    data data_;
};

} // namespace sched
} // namespace dtrace
} // namespace lo2s
