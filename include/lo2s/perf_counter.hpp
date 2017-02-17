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

#include <utility>
#include <cinttypes>

extern "C" {
#include <unistd.h>

#include <linux/perf_event.h>
}

namespace lo2s
{
class perf_counter
{
public:
    perf_counter(pid_t tid, perf_type_id type, uint64_t config, uint64_t config1 = 0);
    perf_counter(const perf_counter&) = delete;
    perf_counter(perf_counter&& other)
    : fd_(-1), previous_(other.previous_), accumulated_(other.accumulated_)
    {
        std::swap(fd_, other.fd_);
    }

    perf_counter& operator=(const perf_counter&) = delete;

    perf_counter& operator=(perf_counter&&) = delete;

    ~perf_counter()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

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
    struct ver
    {
        uint64_t value = 0;
        uint64_t enabled = 0;
        uint64_t running = 0;

        ver operator-(const ver& rhs) const
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
                return (static_cast<double>(value) / running) * value;
            }
            return (static_cast<double>(running) / enabled) * value;
        }
    };
    static_assert(sizeof(ver) == sizeof(uint64_t) * 3, "Your memory layout sucks.");

    int fd_;
    ver previous_;
    double accumulated_ = 0.0;
};
}
