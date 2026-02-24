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

#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>

namespace lo2s::perf::counter::userspace
{
struct UserspaceReadFormat
{
    uint64_t value;
    uint64_t time_enabled;
    uint64_t time_running;
};

class UserspaceCounterBuffer : public CounterBuffer<UserspaceCounterBuffer>
{
public:
    UserspaceCounterBuffer(std::size_t ncounters)
    : CounterBuffer<UserspaceCounterBuffer>(ncounters), current_(ncounters), previous_(ncounters)
    {
    }

    void read(std::vector<UserspaceReadFormat>& inbuf)
    {
        current_ = inbuf;
        update_buffers();
        std::swap(current_, previous_);
    }

    std::size_t diff_enabled(size_t nr)
    {
        return current_[nr].time_enabled - previous_[nr].time_enabled;
    }

    std::size_t diff_running(size_t nr)
    {
        return current_[nr].time_running - previous_[nr].time_running;
    }

    uint64_t diff_value(size_t nr)
    {
        return current_[nr].value - previous_[nr].value;
    }

protected:
    std::vector<UserspaceReadFormat> current_;
    std::vector<UserspaceReadFormat> previous_;
};

} // namespace lo2s::perf::counter::userspace
