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

#include <lo2s/perf/counter/abstract_writer.hpp>
#include <lo2s/perf/counter/reader.hpp>

#include <lo2s/build_config.hpp>
#include <lo2s/config.hpp>

#include <lo2s/perf/event_description.hpp>
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

int perf_try_event_open(struct perf_event_attr* perf_attr, pid_t tid, int cpu, int group_fd,
                        unsigned long flags)
{
    int fd = perf_event_open(perf_attr, tid, cpu, group_fd, flags);
    if (fd < 0 && errno == EACCES && !perf_attr->exclude_kernel && perf_event_paranoid() > 1)
    {
        perf_attr->exclude_kernel = 1;
        perf_warn_paranoid();
        fd = perf_event_open(perf_attr, tid, cpu, group_fd, flags);
    }
    return fd;
}

int open_counter(pid_t tid, int cpuid, const EventDescription& desc, int group_fd)
{
    struct perf_event_attr perf_attr;
    memset(&perf_attr, 0, sizeof(perf_attr));
    perf_attr.size = sizeof(perf_attr);
    perf_attr.sample_period = 0;
    perf_attr.type = desc.type;
    perf_attr.config = desc.config;
    perf_attr.config1 = desc.config1;
    perf_attr.exclude_kernel = config().exclude_kernel;
    // Needed when scaling multiplexed events, and recognize activation phases
    perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

#if !defined(USE_HW_BREAKPOINT_COMPAT) && defined(USE_PERF_CLOCKID)
    perf_attr.use_clockid = config().use_clockid;
    perf_attr.clockid = config().clockid;
#endif

    int fd = perf_try_event_open(&perf_attr, tid, cpuid, group_fd, 0);
    if (fd < 0)
    {
        Log::error() << "perf_event_open for counter failed";
        throw_errno();
    }
    return fd;
}

template <class T>
Reader<T>::Reader(pid_t tid, int cpuid, const CounterCollection& counter_collection,
                  bool enable_on_exec)
: counter_buffer_(counter_collection.counters.size() + 1)
{
    perf_event_attr leader_attr = common_perf_event_attrs();

    leader_attr.type = counter_collection.leader.type;
    leader_attr.config = counter_collection.leader.config;
    leader_attr.config1 = counter_collection.leader.config1;

    leader_attr.sample_type = PERF_SAMPLE_TIME | PERF_SAMPLE_READ;
    leader_attr.freq = config().metric_use_frequency;

    if (leader_attr.freq)
    {
        Log::debug() << "counter::Reader: sample_freq: " << config().metric_frequency;

        leader_attr.sample_freq = config().metric_frequency;
    }
    else
    {
        Log::debug() << "counter::Reader: sample_period: " << config().metric_count;
        leader_attr.sample_period = config().metric_count;
    }

    leader_attr.exclude_kernel = config().exclude_kernel;
    leader_attr.read_format =
        PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_GROUP;
    leader_attr.enable_on_exec = enable_on_exec;

    group_leader_fd_ = perf_try_event_open(&leader_attr, tid, cpuid, -1, 0);
    if (group_leader_fd_ < 0)
    {
        Log::error() << "perf_event_open for counter group leader failed";
        throw_errno();
    }

    Log::debug() << "counter::Reader: leader event: '" << config().metric_leader << "'";

    counter_fds_.reserve(counter_collection.counters.size());
    for (auto& description : counter_collection.counters)
    {
        try
        {
            counter_fds_.emplace_back(open_counter(tid, cpuid, description, group_leader_fd_));
        }
        catch (const std::system_error& e)
        {
            Log::error() << "failed to add counter '" << description.name
                         << "': " << e.code().message();

            if (e.code().value() == EINVAL)
            {
                Log::error()
                    << "opening " << counter_collection.counters.size()
                    << " counters at once might exceed the hardware limit of simultaneously "
                       "openable counters.";
            }
            throw e;
        }
    }

    if (!enable_on_exec)
    {
        auto ret = ::ioctl(group_leader_fd_, PERF_EVENT_IOC_ENABLE);
        if (ret == -1)
        {
            Log::error() << "failed to enable perf counter group";
            ::close(group_leader_fd_);
            throw_errno();
        }
    }
    EventReader<T>::init_mmap(group_leader_fd_);
}

template class Reader<AbstractWriter>;
} // namespace counter
} // namespace perf
} // namespace lo2s
