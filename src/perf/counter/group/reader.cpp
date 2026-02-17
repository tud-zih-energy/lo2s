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

#include <lo2s/perf/counter/group/reader.hpp>

#include <lo2s/build_config.hpp>
#include <lo2s/config.hpp>
#include <lo2s/perf/counter/group/writer.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/util.hpp>

#include <cstring>

extern "C"
{
#include <linux/perf_event.h>
#include <sys/ioctl.h>
}

namespace lo2s
{
namespace perf
{
namespace counter
{
namespace group
{

template <class T>
Reader<T>::Reader(ExecutionScope scope, bool enable_on_exec)
: counter_collection_(
      EventComposer::instance().counters_for(MeasurementScope::group_metric(scope))),
  counter_buffer_(counter_collection_.counters.size() + 1)
{
    Log::debug() << "counter::Reader: leader event: '" << counter_collection_.leader->name() << "'";

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
template class Reader<Writer>;
} // namespace group
} // namespace counter
} // namespace perf
} // namespace lo2s
