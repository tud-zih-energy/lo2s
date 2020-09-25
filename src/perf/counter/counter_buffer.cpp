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

#include <lo2s/perf/counter/counter_buffer.hpp>

#include <cassert>
#include <cstdint>
#include <cstring>

namespace lo2s
{
namespace perf
{
namespace counter
{

CounterBuffer::CounterBuffer(std::size_t ncounters) : accumulated_(ncounters, 0)
{
    buf_ = { std::make_unique<std::byte[]>(GroupReadFormat::total_size(ncounters)),
             std::make_unique<std::byte[]>(GroupReadFormat::total_size(ncounters)) };

    current_ = reinterpret_cast<GroupReadFormat*>(buf_.front().get());
    current_->nr = ncounters;
    current_->time_enabled = 0;
    current_->time_running = 0;
    std::memset(&current_->values, 0, ncounters * sizeof(uint64_t));

    previous_ = reinterpret_cast<GroupReadFormat*>(buf_.back().get());
    previous_->nr = ncounters;
    previous_->time_enabled = 0;
    previous_->time_running = 0;
    std::memset(&current_->values, 0, ncounters * sizeof(uint64_t));
}

void CounterBuffer::read(const GroupReadFormat* inbuf)
{
    assert(accumulated_.size() == inbuf->nr);

    std::memcpy(current_, inbuf, GroupReadFormat::total_size(inbuf->nr));
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

} // namespace counter
} // namespace perf
} // namespace lo2s
