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

#pragma once

#include <lo2s/dtrace/clock.hpp>

#include <otf2xx/chrono/chrono.hpp>

#include <cstdint>

namespace lo2s
{
namespace dtrace
{
namespace time
{

inline dtrace::Clock::time_point convert_time_point(std::uint64_t raw_time)
{
    return dtrace::Clock::time_point(otf2::chrono::nanoseconds(raw_time));
}

class Converter
{
private:
    Converter();

public:
    static Converter& instance()
    {
        static Converter c;
        return c;
    }

    Converter(const Converter&) = default;
    Converter(Converter&&) = default;
    Converter& operator=(const Converter&) = default;
    Converter& operator=(Converter&&) = default;

    otf2::chrono::time_point operator()(std::uint64_t dtrace_raw) const
    {
        return operator()(convert_time_point(dtrace_raw));
    }

    otf2::chrono::time_point operator()(dtrace::Clock::time_point dtrace_tp) const
    {
        return otf2::chrono::time_point(dtrace_tp.time_since_epoch() + offset);
    }

    dtrace::Clock::time_point operator()(otf2::chrono::time_point local_tp) const
    {
        return dtrace::Clock::time_point(local_tp.time_since_epoch() - offset);
    }

private:
    otf2::chrono::duration offset;
};
} // namespace time
} // namespace dtrace
} // namespace lo2s
