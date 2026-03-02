// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/build_config.hpp>

#include <otf2xx/chrono/convert.hpp>
#include <otf2xx/chrono/time_point.hpp>

#include <chrono>
#include <stdexcept> // for use by ClockProvider::InvalidClock
#include <string>    // for use by ClockProvider::get_clock_by_name

#include <ctime>

extern "C"
{
#include <linux/version.h>
}

// All the time stuff is based on the assumption that all times are nanoseconds.
namespace lo2s::time
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
        timespec tp; // NOLINT
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

    static auto& get_descriptions()
    {
        return clocks_;
    }

private:
    static constexpr ClockDescription clocks_[] = {
        {
            "realtime",
            CLOCK_REALTIME,
        },
#ifdef _POSIX_MONOTONIC_CLOCK
        {
            "monotonic",
            CLOCK_MONOTONIC,
        },
#endif
#ifdef _POSIX_CPUTIME
        {
            "process-cputime-id",
            CLOCK_PROCESS_CPUTIME_ID,
        },
#endif
#ifdef _POSIX_THREAD_CPUTIME
        {
            "process-thread-id",
            CLOCK_THREAD_CPUTIME_ID,
        },
#endif
#ifdef HAVE_CLOCK_MONOTONIC_RAW
        {
            "monotonic-raw",
            CLOCK_MONOTONIC_RAW,
        },
#endif
#ifdef HAVE_CLOCK_REALTIME_COARSE
        {
            "realtime-coarse",
            CLOCK_REALTIME_COARSE,
        },
#endif
#ifdef HAVE_CLOCK_MONOTONIC_COARSE
        {
            "monotonic-coarse",
            CLOCK_MONOTONIC_COARSE,
        },
#endif
#ifdef HAVE_CLOCK_BOOTTIME
        {
            "boottime",
            CLOCK_BOOTTIME,
        },
#endif
#ifdef HAVE_CLOCK_REALTIME_ALARM
        {
            "realtime-alarm",
            CLOCK_REALTIME_ALARM,
        },
#endif
#ifdef HAVE_CLOCK_BOOTTIME_ALARM
        {
            "boottime-alarm",
            CLOCK_BOOTTIME_ALARM,
        },
#endif
#ifdef HAVE_CLOCK_MONOTONIC_RAW
        {
            "pebs",
            -1,
        },
    };
#endif
};

inline otf2::chrono::time_point now()
{
    return otf2::chrono::convert_time_point(Clock::now());
}
} // namespace lo2s::time
