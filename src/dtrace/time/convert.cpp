/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

#include <lo2s/dtrace/time/convert.hpp>

#include <lo2s/dtrace/clock.hpp>

#include <lo2s/time/time.hpp>

#include <lo2s/log.hpp>

extern "C"
{
#include <time.h>
}

namespace lo2s
{
namespace dtrace
{
namespace time
{
Converter::Converter() : offset(otf2::chrono::duration(0))
{
    // Dtrace is used with walltimestamp, which is CLOCK_REALTIME.
    // However, in kernel-space it seems to have actually nanoseconds
    // resolution, in user-space it sucks. However, as it has only
    // microseconds resolution in user-space, the offset is very likely
    // zero, which is pretty close to good enough.

    auto local_time = lo2s::time::now();

    struct timespec tp;

    clock_gettime(CLOCK_REALTIME, &tp);

    auto dtrace_time = convert_time_point(static_cast<std::int64_t>(tp.tv_sec * 1e9) +
                                          static_cast<std::int64_t>(tp.tv_nsec));

    offset = local_time.time_since_epoch() - dtrace_time.time_since_epoch();

    Log::debug() << "dtrace time offset: " << offset.count() << "ns ("
                 << local_time.time_since_epoch().count() << "ns - "
                 << dtrace_time.time_since_epoch().count() << "ns).";
}
} // namespace time
} // namespace dtrace
} // namespace lo2s
