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
#ifndef X86_RECORD_OTF2_TIME_HPP
#define X86_RECORD_OTF2_TIME_HPP

#include "log.hpp"
#include "perf_sample_reader.hpp"

#include <otf2xx/chrono/chrono.hpp>

#include <chrono>
#include <thread>

#include <cstdint>
#include <cstdlib>

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

inline perf_clock::time_point perf_tp(uint64_t raw_time)
{
    return perf_clock::time_point(otf2::chrono::nanoseconds(raw_time));
}

inline otf2::chrono::time_point get_time()
{
    return otf2::chrono::convert_time_point(clock::now());
}

class time_reader : public perf_sample_reader<time_reader>
{
public:
    time_reader();

public:
    using perf_sample_reader<time_reader>::handle;
    bool handle(const record_sample_type* sample);

public:
    otf2::chrono::time_point local_time = otf2::chrono::genesis();
    perf_clock::time_point perf_time;
};

class perf_time_converter
{
public:
    perf_time_converter();

    otf2::chrono::time_point operator()(uint64_t perf_raw) const
    {
        return operator()(perf_tp(perf_raw));
    }

    otf2::chrono::time_point operator()(perf_clock::time_point perf_tp) const
    {
        return otf2::chrono::time_point(perf_tp.time_since_epoch() + offset);
    }

    perf_clock::time_point operator()(otf2::chrono::time_point local_tp) const
    {
        return perf_clock::time_point(local_tp.time_since_epoch() - offset);
    }

private:
    otf2::chrono::duration offset;
};
}
#endif // X86_RECORD_OTF2_TIME_HPP
