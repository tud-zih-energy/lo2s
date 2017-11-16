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

#include <lo2s/perf/counter_description.hpp>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

extern "C" {
#include <sys/types.h>
#include <unistd.h>

#include <linux/perf_event.h>
}

namespace lo2s
{
namespace metric
{
class PerfCounter
{
public:
    PerfCounter(pid_t tid, perf_type_id type, std::uint64_t config, std::uint64_t config1,
                int group_fd = -1);

    PerfCounter(const PerfCounter&) = delete;

    PerfCounter(PerfCounter&& other)
    : fd_(-1), previous_(other.previous_), accumulated_(other.accumulated_)
    {
        using std::swap;
        swap(fd_, other.fd_);
    }

    PerfCounter& operator=(const PerfCounter&) = delete;

    PerfCounter& operator=(PerfCounter&&) = delete;

    ~PerfCounter()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

    static int open(pid_t tid, perf_type_id type, std::uint64_t config, std::uint64_t config1,
                    int group_fd);

    double read();

    uint64_t enabled()
    {
        return previous_.enabled;
    }

    uint64_t running()
    {
        return previous_.running;
    }

private:
    struct Ver
    {
        uint64_t value = 0;
        uint64_t enabled = 0;
        uint64_t running = 0;

        Ver operator-(const Ver& rhs) const
        {
            return { value - rhs.value, enabled - rhs.enabled, running - rhs.running };
        }

        double scale() const
        {
            if (running == 0 || running == enabled)
            {
                return value;
            }
            // there is a bug in perf where this is sometimes swapped
            if (enabled > running)
            {
                return (static_cast<double>(enabled) / running) * value;
            }
            return (static_cast<double>(running) / enabled) * value;
        }
    };

    static_assert(sizeof(Ver) == sizeof(uint64_t) * 3, "Your memory layout sucks.");

    int fd_;
    Ver previous_;
    double accumulated_ = 0.0;
};

struct GroupReadFormat
{
    uint64_t nr;
    uint64_t time_enabled;
    uint64_t time_running;
    uint64_t values[1];

    static constexpr std::size_t header_size()
    {
        return sizeof(GroupReadFormat) - sizeof(values);
    }
};

class PerfCounterGroup;

class CounterBuffer
{
public:
    using ReadFormat = GroupReadFormat;

    CounterBuffer(const CounterBuffer&) = delete;
    void operator=(const CounterBuffer&) = delete;

    CounterBuffer(std::size_t ncounters);

    auto operator[](std::size_t i) const
    {
        return accumulated_[i];
    }

    uint64_t enabled() const
    {
        return current_->time_enabled;
    }

    uint64_t running() const
    {
        return current_->time_running;
    }

    void read(int group_leader_fd);
    void read(const ReadFormat* buf);

    std::size_t size() const
    {
        return accumulated_.size();
    }

private:
    void update_buffers();

    static double scale(uint64_t value, uint64_t time_running, uint64_t time_enabled)
    {
        if (time_running == 0 || time_running == time_enabled)
        {
            return value;
        }
        // there is a bug in perf where this is sometimes swapped
        if (time_enabled > time_running)
        {
            return (static_cast<double>(time_enabled) / time_running) * value;
        }
        return (static_cast<double>(time_running) / time_enabled) * value;
    }

    static constexpr std::size_t total_buf_size(std::size_t ncounters)
    {
        return ReadFormat::header_size() + ncounters * sizeof(uint64_t);
    }

    // Double-buffering of read values.  Allows to compute differences between reads
    std::unique_ptr<char[]> buf_;
    ReadFormat* current_;
    ReadFormat* previous_;
    std::vector<double> accumulated_;
};

class PerfCounterGroup
{
public:
    PerfCounterGroup(pid_t tid, const std::vector<perf::CounterDescription>& counter_descs);
    PerfCounterGroup(pid_t tid, const std::vector<perf::CounterDescription>& counter_descs,
                     struct perf_event_attr& leader_attr);

    ~PerfCounterGroup()
    {
        for (int fd : counters_)
        {
            if (fd != -1)
            {
                ::close(fd);
            }
        }
    }

    std::size_t size() const
    {
        return counters_.size();
    }

    auto operator[](std::size_t i) const
    {
        return buf_[i + 1]; // skip group leader counter
    }

    void read()
    {
        buf_.read(group_leader_fd_);
    }

    void read(const CounterBuffer::ReadFormat* inbuf)
    {
        buf_.read(inbuf);
    }

    auto enabled() const
    {
        return buf_.enabled();
    }

    auto running() const
    {
        return buf_.running();
    }

    int group_leader_fd() const
    {
        return group_leader_fd_;
    }

private:
    void add_counter(const perf::CounterDescription& counter);

    int group_leader_fd_;
    pid_t tid_;
    std::vector<int> counters_;
    CounterBuffer buf_;
};
}
}
