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
#include <lo2s/time/time.hpp>

#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/util.hpp>

#include <cstdlib>

extern "C"
{
#include <sys/timerfd.h>
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
  counter_buffer_(counter_collection_.counters.size()), data_(counter_collection_.counters.size())
{
    struct itimerspec tspec;
    memset(&tspec, 0, sizeof(struct itimerspec));
    tspec.it_value.tv_nsec = 1;

    tspec.it_interval.tv_sec =
        std::chrono::duration_cast<std::chrono::seconds>(config().userspace_read_interval).count();

    tspec.it_interval.tv_nsec =
        (config().userspace_read_interval % std::chrono::seconds(1)).count();
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    timerfd_settime(timer_fd_, TFD_TIMER_ABSTIME, &tspec, NULL);

    for (auto& event : counter_collection_.counters)
    {
        counter_fds_.emplace_back(perf_event_description_open(scope, event, -1));
    }
}

template <class T>
void Reader<T>::read()
{
    for (std::size_t i = 0; i < counter_fds_.size(); i++)
    {
        [[maybe_unused]] auto bytes_read =
            ::read(counter_fds_[i], &(data_[i]), sizeof(UserspaceReadFormat));

        assert(bytes_read == sizeof(UserspaceReadFormat));
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
