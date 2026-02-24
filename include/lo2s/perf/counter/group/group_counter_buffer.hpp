/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is freei software: you can redistribute it and/or modify
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

#include <lo2s/perf/counter/counter_buffer.hpp>

#include <array>
#include <memory>
#include <utility>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lo2s::perf::counter::group
{
struct GroupReadFormat
{
    uint64_t nr;
    uint64_t time_enabled;
    uint64_t time_running;
    uint64_t values[1];

    static constexpr std::size_t total_size(std::size_t ncounters)
    {
        return sizeof(GroupReadFormat) + ((ncounters - 1) * sizeof(uint64_t));
    }
};

class GroupCounterBuffer : public CounterBuffer<GroupCounterBuffer>
{
public:
    GroupCounterBuffer(std::size_t ncounters)
    : CounterBuffer<GroupCounterBuffer>(ncounters),
      buf_({ std::make_unique<std::byte[]>(GroupReadFormat::total_size(ncounters)),
             std::make_unique<std::byte[]>(GroupReadFormat::total_size(ncounters)) }),
      current_(reinterpret_cast<GroupReadFormat*>(buf_.front().get())),
      previous_(reinterpret_cast<GroupReadFormat*>(buf_.back().get()))
    {
        current_->nr = ncounters;
        current_->time_enabled = 0;
        current_->time_running = 0;
        std::memset(&current_->values, 0, ncounters * sizeof(uint64_t));

        previous_->nr = ncounters;
        previous_->time_enabled = 0;
        previous_->time_running = 0;
        std::memset(&current_->values, 0, ncounters * sizeof(uint64_t));
    }

    uint64_t enabled() const
    {
        return previous_->time_enabled;
    }

    uint64_t running() const
    {
        return previous_->time_running;
    }

    void read(const GroupReadFormat* inbuf)
    {
        assert(accumulated_.size() == inbuf->nr);
        assert(current_->nr == previous_->nr);

        std::memcpy(current_, inbuf, GroupReadFormat::total_size(inbuf->nr));
        update_buffers();
        // Swap double buffers.  `current_` now points to the oldest buffer.  Each
        // call to this->read(...) stores the new counter values in `current_`, so
        // only the oldest data is overwritten.
        std::swap(current_, previous_);
    }

    std::size_t enabled()
    {
        return previous_->time_enabled;
    }

    std::size_t running()
    {
        return previous_->time_running;
    }

    std::size_t diff_enabled([[maybe_unused]] size_t nr)
    {
        return current_->time_enabled - previous_->time_enabled;
    }

    std::size_t diff_running([[maybe_unused]] size_t nr)
    {
        return current_->time_running - previous_->time_running;
    }

    uint64_t diff_value(size_t nr)
    {
        return current_->values[nr] - previous_->values[nr];
    }

protected:
    // Double-buffering of read values.  Allows to compute differences between reads
    std::array<std::unique_ptr<std::byte[]>, 2> buf_;
    GroupReadFormat* current_;
    GroupReadFormat* previous_;
};

} // namespace lo2s::perf::counter::group
