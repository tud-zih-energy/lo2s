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

#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/perf/event_reader.hpp>

#include <stdexcept>
#include <system_error>

#include <cstdint>

#include <fmt/format.h>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf::sample
{
template <class T>
class Reader : public EventReader<T>
{
public:
    struct RecordSampleType
    {
        // BAD things happen if you try this
        RecordSampleType() = delete;
        RecordSampleType(const RecordSampleType&) = delete;
        RecordSampleType& operator=(const RecordSampleType&) = delete;
        RecordSampleType(RecordSampleType&&) = delete;
        RecordSampleType& operator=(RecordSampleType&&) = delete;
        ~RecordSampleType() = default;

        struct perf_event_header header;
        uint64_t ip;
        uint32_t pid, tid;
        uint64_t time;
        uint32_t cpu, res;
        /* only relevant for record_callgraph_ / PERF_SAMPLE_CALLCHAIN */
        uint64_t nr;
        uint64_t ips[1]; // ISO C++ forbits zero-size array
    };

    Reader(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader& operator=(Reader&&) = delete;

protected:
    using EventReader<T>::init_mmap;

    ~Reader()
    {
        if ((this->total_samples > 0) &&
            ((this->throttle_samples * 100) / this->total_samples) > 20)
        {
            Log::warn() << "The given sample period is too low and the kernel assumes it has an "
                           "overwhelming influence on the runtime!\n"
                           "Thus, the kernel increased and decreased this period often "
                           "(> 20 % of the throttling events, < 80 % sampling events)."
                           "You can increase /proc/sys/kernel/perf_event_max_sample_rate to some "
                           "extend to prevent the kernel from doing that"
                           " or you can increase your sampling period.";
        }
    }

    bool record_callgraph_;

private:
    static EventGuard create_sampling_event(ExecutionScope scope, bool enable_on_exec)
    {
        Log::debug() << "initializing event_reader for:" << scope.name()
                     << ", enable_on_exec: " << enable_on_exec;
        try
        {
            EventAttr event = EventComposer::instance().create_sampling_event();
            if (enable_on_exec)
            {
                event.set_enable_on_exec();
            }
            else
            {
                event.set_disabled();
            }

            return event.open(scope, config().perf.cgroup_fd);
        }
        catch (std::system_error& e)
        {
            throw std::runtime_error(fmt::format("Could not open sampling event '{}' for {}: {}",
                                                 config().perf.sampling.event, scope.name(),
                                                 e.what()));
        }
    }

    Reader(ExecutionScope scope, bool enable_on_exec)
    : record_callgraph_(config().perf.sampling.enable_callgraph),
      event_(create_sampling_event(scope, enable_on_exec))
    {
        init_mmap(event_.get_fd());
        Log::debug() << "mmap initialized";

        if (!enable_on_exec)
        {
            event_.enable();
        }
    }

    friend T;
    EventGuard event_;
};
} // namespace lo2s::perf::sample
