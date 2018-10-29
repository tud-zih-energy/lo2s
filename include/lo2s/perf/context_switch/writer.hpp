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

#include <lo2s/perf/context_switch/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/calling_context.hpp>
#include <otf2xx/writer/local.hpp>
namespace lo2s
{
namespace perf
{
namespace context_switch
{

class Writer : public Reader<Writer>
{
public:
    Writer(int cpu, trace::Trace& trace);
    ~Writer();

    using Reader<Writer>::handle;
    bool handle(const Reader::RecordSwitchCpuWideType* context_switch);

private:
    otf2::writer::local& otf2_writer_;
    const time::Converter time_converter_;
    trace::Trace& trace_;
    std::unordered_map<pid_t, otf2::definition::calling_context::reference_type>
        thread_calling_context_refs_;

    otf2::definition::calling_context::reference_type last_calling_context_ =
        otf2::definition::calling_context::reference_type::undefined();
    otf2::chrono::time_point last_time_point_;

    enum class LastEventType
    {
        enter,
        leave
    };
    LastEventType last_event_type_ = LastEventType::leave;
};
} // namespace context_switch
} // namespace perf
} // namespace lo2s
