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
#include <lo2s/perf/counter/group/writer.hpp>

#include <lo2s/build_config.hpp>
#include <lo2s/config.hpp>

#include <lo2s/perf/event.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/util.hpp>

#include <cstring>

extern "C"
{
#include <sys/ioctl.h>

#include <linux/perf_event.h>
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
      CounterProvider::instance().collection_for(MeasurementScope::group_metric(scope))),
  counter_buffer_(counter_collection_.counters.size() + 1)
{
    if (config().metric_use_frequency)
    {
        counter_collection_.leader.sample_freq(config().metric_frequency);
    }
    else
    {
        counter_collection_.leader.sample_period(config().metric_count);
    }

    do
    {
        try
        {
            counter_leader_ =
                counter_collection_.leader.open_as_group_leader(scope, config().cgroup_fd);
        }
        catch (const std::system_error& e)
        {
            // perf_try_event_open was used here before
            if (counter_leader_.value().get_fd() < 0 && errno == EACCES &&
                !counter_collection_.leader.attr().exclude_kernel && perf_event_paranoid() > 1)
            {
                counter_collection_.leader.mut_attr().exclude_kernel = 1;
                perf_warn_paranoid();

                continue;
            }

            if (!counter_leader_.value().is_valid())
            {
                Log::error() << "perf_event_open for counter group leader failed";
                throw_errno();
            }
        }
    } while (!counter_leader_.value().is_valid());

    Log::debug() << "counter::Reader: leader event: '" << counter_collection_.leader.name() << "'";

    for (auto& counter_ev : counter_collection_.counters)
    {
        if (counter_ev.is_available_in(scope))
        {
            std::optional<EventGuard> counter = std::nullopt;
            counter_ev.mut_attr().exclude_kernel = counter_collection_.leader.attr().exclude_kernel;
            do
            {
                try
                {
                    counter = counter_leader_.value().open_child(counter_ev, scope);
                }
                catch (const std::system_error& e)
                {
                    if (!counter.value().is_valid())
                    {
                        Log::error() << "failed to add counter '" << counter_ev.name()
                                     << "': " << e.code().message();

                        if (e.code().value() == EINVAL)
                        {
                            Log::error() << "opening " << counter_collection_.counters.size()
                                         << " counters at once might exceed the hardware limit of "
                                            "simultaneously "
                                            "openable counters.";
                        }
                        throw e;
                    }
                    counters_.emplace_back(std::move(counter.value()));
                }
            } while (!counter.value().is_valid());
        }
    }

    if (!enable_on_exec)
    {
        counter_leader_.value().enable();
    }

    EventReader<T>::init_mmap(counter_leader_.value().get_fd());
}
template class Reader<Writer>;
} // namespace group
} // namespace counter
} // namespace perf
} // namespace lo2s
