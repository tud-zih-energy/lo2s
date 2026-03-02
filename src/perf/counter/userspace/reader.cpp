// SPDX-FileCopyrightText: 2021 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/perf/counter/userspace/reader.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/userspace/userspace_counter_buffer.hpp>
#include <lo2s/perf/counter/userspace/writer.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/util.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdlib>

extern "C"
{
#include <unistd.h>
}

namespace lo2s::perf::counter::userspace
{
template <class T>
Reader<T>::Reader(ExecutionScope scope)
: counter_collection_(
      EventComposer::instance().counters_for(MeasurementScope::userspace_metric(scope))),
  counter_buffer_(counter_collection_.counters.size()),
  timer_fd_(timerfd_from_ns(config().perf.userspace.read_interval)),
  data_(counter_collection_.counters.size())
{
    for (auto& event : counter_collection_.counters)
    {
        counters_.emplace_back(event.open(scope));
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

    [[maybe_unused]] uint64_t expirations = 0;
    if (::read(timer_fd_, &expirations, sizeof(expirations)) == -1 && errno != EAGAIN)
    {
        Log::error() << "Flushing timer fd failed";
        throw_errno();
    }
}

template class Reader<Writer>;
} // namespace lo2s::perf::counter::userspace
