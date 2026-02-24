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

#include <cstddef>

namespace lo2s::perf::counter
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
    CounterBuffer(CounterBuffer&) = delete;
    CounterBuffer& operator=(CounterBuffer&) = delete;

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
            // Only a limited amount of hardware counters can be recorded per-CPU.
            // If a user opens more counters than the processor can handle, perf multiplexes the
            // hardware counters by running each of them  for just a part of the overall
            // runtime. For users of perf to be able to account for this multiplexing, perf keeps
            // track of two statistics per counter:
            //
            // diff_enabled - time the counter was enabled and _could_ have run
            // diff_running - time the counter _actually_ ran
            //
            // Using diff_enabled / diff_running as a scaling factor, one can estimate the value for
            // the counter if it ran for the whole diff_enabled duration.
            //
            // So for example, if the counter was only active for half of the time, a scaling factor
            // of 2 would be applied.
            //
            // Due to floating point edge cases, several combinations of diff_enabled, diff_running
            // and diff_value where some of those are 0 can result in NaNs, which due to the
            // accumulated nature of counters will break them.
            //
            // A counter reading of 0 is assumed in all of these cases, as diff_enabled or
            // diff_running being 0 indicates that the counter did not run during the last sampling
            // interval

            auto diff_enabled = crtp_this->diff_enabled(i);
            auto diff_running = crtp_this->diff_running(i);
            auto diff_value = crtp_this->diff_value(i);

            if (diff_enabled == 0 || diff_running == 0 || diff_value == 0)
            {
                continue;
            }

            if (diff_enabled == diff_running)
            {
                accumulated_[i] += diff_value;
            }
            else
            {
                accumulated_[i] += (static_cast<double>(diff_enabled) / diff_running) * diff_value;
            }
        }
    }

private:
    friend T;
    CounterBuffer& operator=(CounterBuffer&&) = default;
    CounterBuffer(CounterBuffer&&) = default;
    ~CounterBuffer() = default;

    CounterBuffer(std::size_t ncounters) : accumulated_(ncounters, 0)
    {
    }

    std::vector<double> accumulated_;
};

} // namespace lo2s::perf::counter
