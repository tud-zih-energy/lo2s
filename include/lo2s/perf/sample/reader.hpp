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

#include <lo2s/perf/event_composer.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/util.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

#include <stdexcept>

#include <cstdlib>
#include <cstring>

extern "C"
{
#include <fcntl.h>
#include <unistd.h>

#include <linux/perf_event.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
}

namespace lo2s
{
namespace perf
{

namespace sample
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

        struct perf_event_header header;
        uint64_t ip;
        uint32_t pid, tid;
        uint64_t time;
        uint32_t cpu, res;
        /* only relevant for has_cct_ / PERF_SAMPLE_CALLCHAIN */
        uint64_t nr;
        uint64_t ips[1]; // ISO C++ forbits zero-size array
    };

protected:
    using EventReader<T>::init_mmap;

    Reader(ExecutionScope scope, bool enable_on_exec) : has_cct_(config().enable_cct)
    {
        Log::debug() << "initializing event_reader for:" << scope.name()
                     << ", enable_on_exec: " << enable_on_exec;

        EventAttr event = EventComposer::instance().create_sampling_event();
        if (enable_on_exec)
        {
            event.set_enable_on_exec();
        }
        else
        {
            event.set_disabled();
        }

        event_ = event.open(scope, config().cgroup_fd);

        try
        {
            init_mmap(event_.value().get_fd());
            Log::debug() << "mmap initialized";

            if (!enable_on_exec)
            {
                event_.value().enable();
            }
        }
        catch (...)
        {
            throw;
        }
    }

    Reader(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader& operator=(Reader&&) = delete;

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

protected:
    bool has_cct_;

private:
    std::optional<EventGuard> event_;
};
} // namespace sample
} // namespace perf
} // namespace lo2s
