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

#include <vector>

namespace lo2s
{
namespace perf
{
namespace counter
{

/* when reading perf counters, the counter value is reported as the amount of events since
 * the start of measurement, while we want the amount of events since the previous read.
 *
 * To get the amount of events in regards to the previous read, this class buffers the previous read
 * result to calculate the difference
 */

template <class T>
class CounterBuffer
{
public:
    CounterBuffer(const CounterBuffer&) = delete;
    void operator=(const CounterBuffer&) = delete;

    CounterBuffer(std::size_t ncounters) : accumulated_(ncounters, 0)
    {
    }

    auto operator[](std::size_t i) const
    {
        return accumulated_[i];
    }

    std::size_t size() const
    {
        return accumulated_.size();
    }

protected:
    void update_buffers()
    {
        auto crtp_this = static_cast<T*>(this);

        for (std::size_t i = 0; i < accumulated_.size(); ++i)
        {
            auto diff_enabled = crtp_this->diff_enabled(i);
            auto diff_running = crtp_this->diff_running(i);
            auto diff_value = crtp_this->diff_value(i);

            if (diff_enabled == 0 || diff_running == diff_enabled)
            {
                accumulated_[i] += diff_value;
            }
            // Due to a perf bug diff_enabled and diff_running may be swapped
            // diff_enabled is always smaller than diff_running so swap them if diff_enabled >
            // diff_running
            else if (diff_enabled > diff_running)
            {
                accumulated_[i] += (static_cast<double>(diff_enabled) / diff_running) * diff_value;
            }
            else
            {
                accumulated_[i] += (static_cast<double>(diff_running) / diff_enabled) * diff_value;
            }
        }
    }
    std::vector<double> accumulated_;
};

} // namespace counter
} // namespace perf
} // namespace lo2s
