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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace lo2s
{
namespace perf
{
namespace counter
{

struct GroupReadFormat
{
    uint64_t nr;
    uint64_t time_enabled;
    uint64_t time_running;
    uint64_t values[1];

    static constexpr std::size_t total_size(std::size_t ncounters)
    {
        return sizeof(GroupReadFormat) + (ncounters - 1) * sizeof(uint64_t);
    }
};

/* when reading perf counters, the counter value is reported as the amount of events since
 * the start of measurement, while we want the amount of events since the previous read.
 *
 * To get the amount of events in regards to the previous read, this class buffers the previous read
 * result to calculate the difference
 */
class CounterBuffer
{
public:
    CounterBuffer(const CounterBuffer&) = delete;
    void operator=(const CounterBuffer&) = delete;

    CounterBuffer(std::size_t ncounters);

    auto operator[](std::size_t i) const
    {
        return accumulated_[i];
    }

    uint64_t enabled() const
    {
        return previous_->time_enabled;
    }

    uint64_t running() const
    {
        return previous_->time_running;
    }

    void read(const GroupReadFormat* buf);

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

    // Double-buffering of read values.  Allows to compute differences between reads
    std::array<std::unique_ptr<std::byte[]>, 2> buf_;
    GroupReadFormat* current_;
    GroupReadFormat* previous_;
    std::vector<double> accumulated_;
};

} // namespace counter
} // namespace perf
} // namespace lo2s
