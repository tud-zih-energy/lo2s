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

#include <lo2s/time/time.hpp>

#include <string>

namespace lo2s::time
{
clockid_t Clock::clockid_ = CLOCK_MONOTONIC_RAW;

const ClockDescription& ClockProvider::get_clock_by_name(const std::string& name)
{
    for (const auto& clock : clocks_)
    {
        if (name == clock.name)
        {
            return clock;
        }
    }

    using namespace std::literals::string_literals;
    throw InvalidClock("clock \'"s + name + "\' is not available"s);
}
} // namespace lo2s::time
