/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2021,
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

#include <lo2s/error.hpp>
#include <lo2s/perf/counter/userspace/reader.hpp>
#include <lo2s/perf/counter/userspace/writer.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/time/time.hpp>

#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/util.hpp>

#include <cstdlib>

extern "C"
{
#include <unistd.h>
}

namespace lo2s
{
namespace perf
{
namespace counter
{
namespace userspace
{
template <class T>
Reader<T>::Reader(ExecutionScope scope)
: counter_collection_(
      EventComposer::instance().counters_for(MeasurementScope::userspace_metric(scope))),
  counter_buffer_(counter_collection_.counters.size()),
  timer_fd_(TimerFd::create(config().userspace_read_interval).unpack_ok()),
  data_(counter_collection_.counters.size())
{
    for (auto& event : counter_collection_.counters)
    {
        counters_.emplace_back(event.open(scope).unpack_ok());
    }
}

template <class T>
void Reader<T>::read()
{
    for (std::size_t i = 0; i < counters_.size(); i++)
    {
        data_[i] = counters_[i].read<UserspaceReadFormat>().unpack_ok();
    }

    static_cast<T*>(this)->handle(data_);

    timer_fd_.reset();
}

template class Reader<Writer>;
} // namespace userspace
} // namespace counter
} // namespace perf
} // namespace lo2s
