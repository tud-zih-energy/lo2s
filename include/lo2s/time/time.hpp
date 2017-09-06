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

#include <lo2s/log.hpp>

#include <otf2xx/chrono/chrono.hpp>

#include <chrono>
#include <ctime>

#include <cstdint>

#include <stdexcept> // for use by ClockProvider::InvalidClock
#include <string>    // for use by ClockProvider::get_clock_by_name

// All the time stuff is based on the assumption that all times are nanoseconds.
namespace lo2s
{
namespace time
{

struct Clock
{
    // This is except from the type = CamelCase naming convention to be compatible
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<Clock, duration>;

    static time_point now() noexcept
    {
        timespec tp;
        clock_gettime(clockid_, &tp);
        return time_point(duration(std::chrono::seconds(tp.tv_sec)) +
                          duration(std::chrono::nanoseconds(tp.tv_nsec)));
    }

    static void set_clock(clockid_t id)
    {
        clockid_ = id;
    }

private:
    static clockid_t clockid_;
};

struct ClockDescription
{
    const char* name;
    clockid_t id;
};

class ClockProvider
{
public:
    struct InvalidClock : std::runtime_error
    {
        InvalidClock(const std::string& what) : std::runtime_error(what)
        {
        }
    };

    static const ClockDescription& get_clock_by_name(const std::string& name);

private:
    static constexpr ClockDescription clocks_[] = {
        {
            "realtime", CLOCK_REALTIME,
        },
        {
            "monotonic", CLOCK_MONOTONIC,
        },
        {
            "process-cputime-id", CLOCK_PROCESS_CPUTIME_ID,
        },
        {
            "process-thread-id", CLOCK_THREAD_CPUTIME_ID,
        },
        {
            "monotonic-raw", CLOCK_MONOTONIC_RAW,
        },
        {
            "realtime-coarse", CLOCK_REALTIME_COARSE,
        },
        {
            "monotonic-coarse", CLOCK_MONOTONIC_COARSE,
        },
        {
            "boottime", CLOCK_BOOTTIME,
        },
        {
            "realtime-alarm", CLOCK_REALTIME_ALARM,
        },
        {
            "boottime-alarm", CLOCK_BOOTTIME_ALARM,
        },
        {
            "tai", CLOCK_TAI,
        }
    };
};

inline otf2::chrono::time_point now()
{
    return otf2::chrono::convert_time_point(Clock::now());
}
}
}
