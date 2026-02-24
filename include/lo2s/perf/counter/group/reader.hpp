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

#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/counter/group/group_counter_buffer.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/perf/event_reader.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

#include <cstdint>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf::counter::group
{

// This class is concerned with setting up and reading out perf counters in a group.
// This group has a group leader event, which triggers a readout of the other
// events every --metric-count occurences. The value is then written into a memory-mapped
// ring buffer, which we read out routinely to get the counter values.
template <class T>
class Reader : public EventReader<T>
{
public:
    Reader(ExecutionScope scope, bool enable_on_exec)
    : counter_collection_(
          EventComposer::instance().counters_for(MeasurementScope::group_metric(scope))),
      counter_buffer_(counter_collection_.counters.size() + 1)
    {
        if (!counter_collection_.leader.has_value())
        {
            throw std::runtime_error(
                "Invalid Counter Collection: Counter collection has no leader!");
        }

        Log::debug() << "counter::Reader: leader event: '" << counter_collection_.leader->name()
                     << "'";

        if (enable_on_exec)
        {
            counter_collection_.leader->set_enable_on_exec();
        }
        else
        {
            counter_collection_.leader->set_disabled();
        }

        counter_leader_ = counter_collection_.leader->open(scope, config().cgroup_fd);

        for (auto& counter_ev : counter_collection_.counters)
        {
            counters_.emplace_back(counter_leader_->open_child(counter_ev, scope));
        }

        if (!enable_on_exec)
        {
            counter_leader_->enable();
        }

        EventReader<T>::init_mmap(counter_leader_.value().get_fd());
    }

    struct RecordSampleType
    {
        struct perf_event_header header;
        uint64_t time;
        struct GroupReadFormat v;
    };

protected:
    std::optional<EventGuard> counter_leader_;
    std::vector<EventGuard> counters_;
    CounterCollection counter_collection_;
    GroupCounterBuffer counter_buffer_;
};
} // namespace lo2s::perf::counter::group
