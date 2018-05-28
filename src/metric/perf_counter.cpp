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

#include <lo2s/metric/perf_counter.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/util.hpp>

#include <cstdint>
#include <cstring>

extern "C"
{
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/perf_event.h>
}

namespace
{
static int perf_try_event_open(struct perf_event_attr* perf_attr, pid_t tid, int cpu, int group_fd,
                               unsigned long flags)
{
    int fd = lo2s::perf::perf_event_open(perf_attr, tid, cpu, group_fd, flags);
    if (fd < 0 && errno == EACCES && !perf_attr->exclude_kernel &&
        lo2s::perf::perf_event_paranoid() > 1)
    {
        perf_attr->exclude_kernel = 1;
        lo2s::Log::warn() << "kernel.perf_event_paranoid > 1, retrying without kernel samples:";
        lo2s::Log::warn() << " * sysctl kernel.perf_event_paranoid=1";
        lo2s::Log::warn() << " * run lo2s as root";
        lo2s::Log::warn() << " * run with --no-kernel to disable kernel space monitoring in "
                             "the first place,";
        fd = lo2s::perf::perf_event_open(perf_attr, tid, cpu, group_fd, flags);
    }
    return fd;
}
} // namespace

namespace lo2s
{
namespace metric
{
PerfCounter::PerfCounter(pid_t tid, int cpuid, perf_type_id type, std::uint64_t config,
                         std::uint64_t config1, int group_fd)
: fd_(open(tid, cpuid, type, config, config1, group_fd))
{
}

int PerfCounter::open(pid_t tid, int cpuid, perf_type_id type, std::uint64_t config,
                      std::uint64_t config1, int group_fd)
{
    struct perf_event_attr perf_attr;
    memset(&perf_attr, 0, sizeof(perf_attr));
    perf_attr.size = sizeof(perf_attr);
    perf_attr.sample_period = 0;
    perf_attr.type = type;
    perf_attr.config = config;
    perf_attr.config1 = config1;
    perf_attr.exclude_kernel = lo2s::config().exclude_kernel;
    // Needed when scaling multiplexed events, and recognize activation phases
    perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

#if !defined(HW_BREAKPOINT_COMPAT) && defined(USE_PERF_CLOCKID)
    perf_attr.use_clockid = lo2s::config().use_clockid;
    perf_attr.clockid = lo2s::config().clockid;
#endif

    int fd = perf_try_event_open(&perf_attr, tid, cpuid, group_fd, 0);
    if (fd < 0)
    {
        Log::error() << "perf_event_open for counter failed";
        throw_errno();
    }
    return fd;
}

double PerfCounter::read()
{
    Ver infos;
    auto res = ::read(fd_, &infos, sizeof(infos));
    if (res != sizeof(infos))
    {
        Log::error() << "could not read counter values";
        throw_errno();
    }
    double value = (infos - previous_).scale() + accumulated_;
    previous_ = infos;
    accumulated_ = value;
    return accumulated_;
}

CounterBuffer::CounterBuffer(std::size_t ncounters)
: buf_(std::make_unique<char[]>(2 * total_buf_size(ncounters))), accumulated_(ncounters, 0)
{
    // `buf_` points to contiguous memory that holds both entries of the double
    // buffer;  let the non-owning pointers `current_` and `previous_` point to
    // the correct offsets in the former.
    current_ = reinterpret_cast<ReadFormat*>(buf_.get());
    current_->nr = ncounters;
    current_->time_enabled = 0;
    current_->time_running = 0;
    std::memset(&current_->values, 0, ncounters * sizeof(uint64_t));

    previous_ = reinterpret_cast<ReadFormat*>(buf_.get() + total_buf_size(ncounters));
    previous_->nr = ncounters;
    previous_->time_enabled = 0;
    previous_->time_running = 0;
    std::memset(&current_->values, 0, ncounters * sizeof(uint64_t));
}

void CounterBuffer::read(int group_leader_fd)
{
    auto ncounters = previous_->nr;
    assert(accumulated_.size() == ncounters);

    auto bufsz = total_buf_size(ncounters);

    auto read_bytes = ::read(group_leader_fd, current_, bufsz);
    if (read_bytes < 0 || static_cast<decltype(bufsz)>(read_bytes) != bufsz)
    {
        Log::error() << "failed to read counter buffer from file descriptor";
        throw_errno();
    }

    update_buffers();
}

void CounterBuffer::read(const ReadFormat* inbuf)
{
    assert(accumulated_.size() == inbuf->nr);

    std::memcpy(current_, inbuf, total_buf_size(inbuf->nr));
    update_buffers();
}

void CounterBuffer::update_buffers()
{
    // Assert that we aren't doing any out-of-bounds reads/writes.
    assert(current_->nr == previous_->nr);
    assert(accumulated_.size() == current_->nr);

    // Compute difference between reads for scaling multiplexed events
    auto diff_enabled = current_->time_enabled - previous_->time_enabled;
    auto diff_running = current_->time_running - previous_->time_running;
    for (std::size_t i = 0; i < current_->nr; ++i)
    {
        accumulated_[i] +=
            scale(current_->values[i] - previous_->values[i], diff_running, diff_enabled);
    }

    // Swap double buffers.  `current_` now points to the oldest buffer.  Each
    // call to this->read(...) stores the new counter values in `current_`, so
    // only the oldest data is overwritten.
    std::swap(current_, previous_);
}

PerfCounterGroup::PerfCounterGroup(pid_t tid, int cpuid,
                                   const std::vector<perf::CounterDescription>& counter_descs,
                                   perf_event_attr leader_attr, bool enable_on_exec)
: tid_(tid), cpuid_(cpuid), buf_(counter_descs.size() + 1 /* add group leader counter */)
{
    leader_attr.size = sizeof(leader_attr);
    leader_attr.disabled = 1; // disable until all events in the group are set up
    leader_attr.exclude_kernel = lo2s::config().exclude_kernel;
    leader_attr.read_format =
        PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_GROUP;
    leader_attr.enable_on_exec = enable_on_exec;

    group_leader_fd_ = perf_try_event_open(&leader_attr, tid_, cpuid_, -1, 0);
    if (group_leader_fd_ < 0)
    {
        Log::error() << "perf_event_open for counter group leader failed";
        throw_errno();
    }

    counters_.reserve(counter_descs.size());
    for (auto& description : counter_descs)
    {
        try
        {
            add_counter(description);
        }
        catch (const std::system_error& e)
        {
            Log::error() << "failed to add counter '" << description.name
                         << "': " << e.code().message();

            if (e.code().value() == EINVAL)
            {
                Log::error()
                    << "opening " << counter_descs.size()
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
}

void PerfCounterGroup::add_counter(const perf::CounterDescription& counter)
{
    counters_.emplace_back(PerfCounter::open(tid_, cpuid_, counter.type, counter.config,
                                             counter.config1, group_leader_fd_));
}
} // namespace metric
} // namespace lo2s
