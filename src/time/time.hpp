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

#ifndef LO2S_TIME_HPP
#define LO2S_TIME_HPP

#include "../log.hpp"

#include <otf2xx/chrono/chrono.hpp>

#include <chrono>

#include <cstdint>

// All the time stuff is based on the assumption that all times are nanoseconds.
namespace lo2s
{

using clock = std::chrono::steady_clock;

struct perf_clock
{
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<perf_clock, duration>;
};

inline perf_clock::time_point perf_tp(std::uint64_t raw_time)
{
    return perf_clock::time_point(otf2::chrono::nanoseconds(raw_time));
}

inline otf2::chrono::time_point get_time()
{
    return otf2::chrono::convert_time_point(clock::now());
}
}
#endif // LO2S_TIME_HPP
