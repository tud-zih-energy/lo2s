/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/tracepoint/event.hpp>

#include <optional>
#include <vector>

namespace lo2s
{
namespace perf
{
namespace counter
{
class CounterProvider
{
public:
    CounterProvider()
    {
    }

    static CounterProvider& instance()
    {
        static CounterProvider provider;
        return provider;
    }

    void initialize_group_counters(const std::string& leader,
                                   const std::vector<std::string>& counters);
    void initialize_userspace_counters(const std::vector<std::string>& counters);
    void initialize_tracepoints(const std::vector<std::string>& tracepoints);

    bool has_group_counters(ExecutionScope scope);
    bool has_userspace_counters(ExecutionScope scope);

    CounterCollection collection_for(MeasurementScope scope);
    std::vector<std::string> get_tracepoint_event_names();

private:
    std::optional<Event> group_leader_;
    std::vector<Event> group_events_;
    std::vector<Event> userspace_events_;
    std::vector<tracepoint::TracepointEvent> tracepoint_events_;
};
} // namespace counter
} // namespace perf
} // namespace lo2s
