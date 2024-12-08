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
#include <lo2s/perf/event.hpp>
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
      CounterProvider::instance().collection_for(MeasurementScope::userspace_metric(scope))),
  counter_buffer_(counter_collection_.counters.size()),
  timer_fd_(timerfd_from_ns(config().userspace_read_interval)),
  data_(counter_collection_.counters.size())
{
    for (auto& event : counter_collection_.counters)
    {
        std::optional<EventGuard> counter;

        try
        {
            counter = event.open(scope);
        }
        catch (const std::system_error& e)
        {
            // perf_try_event_open was used here before
            if (counter.value().get_fd() < 0 && errno == EACCES && !event.attr().exclude_kernel &&
                perf_event_paranoid() > 1)
            {
                event.mut_attr().exclude_kernel = 1;
                perf_warn_paranoid();

                counter = event.open(scope);
            }

            if (!counter.value().is_valid())
            {
                Log::error() << "perf_event_open for counter failed";
                throw_errno();
            }
            else
            {
                counters_.emplace_back(std::move(counter.value()));
            }
        }
    }
}

template <class T>
void Reader<T>::read()
{
    for (std::size_t i = 0; i < counters_.size(); i++)
    {
        data_[i] = counters_[i].read<UserspaceReadFormat>();
    }

    static_cast<T*>(this)->handle(data_);

    [[maybe_unused]] uint64_t expirations;
    if (::read(timer_fd_, &expirations, sizeof(expirations)) == -1 && errno != EAGAIN)
    {
        Log::error() << "Flushing timer fd failed";
        throw_errno();
    }
}

template class Reader<Writer>;
} // namespace userspace
} // namespace counter
} // namespace perf
} // namespace lo2s
