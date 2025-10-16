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

#include "otf2xx/chrono/duration.hpp"
#include <lo2s/log.hpp>
#include <lo2s/perf/time/converter.hpp>

namespace lo2s
{
namespace perf
{
namespace time
{
Converter::Converter() : offset(otf2::chrono::duration(0))
{
    if (config().use_perf())
    {
        Reader reader;
        reader.read();

        if (reader.perf_time.time_since_epoch().count() == 0)
        {
            Log::error()
                << "Could not determine perf_time offset. Synchronization event was not triggered.";
            return;
        }

        // we expect local_time <= perf_time, i.e. time_diff < 0
        const auto time_diff =
            reader.local_time.time_since_epoch() - reader.perf_time.time_since_epoch();

        if (lo2s::config().clockid.has_value())
        {
            if (time_diff < std::chrono::microseconds(-100) or
                time_diff > std::chrono::microseconds(0))
            {
                Log::warn() << "Unusually large perf time offset detected after synchronization! ("
                            << std::showpos << time_diff.count() << std::noshowpos << "ns)";
                offset = time_diff;
            }
        }
        else
        {
            offset = time_diff;
        }
        Log::debug() << "perf time offset: " << time_diff.count() << "ns ("
                     << reader.local_time.time_since_epoch().count() << "ns - "
                     << reader.perf_time.time_since_epoch().count() << "ns).";
    }
    else
    {
        // If we are not using any perf features at all, there is no sense in synchronizing
        // the perf-internal and external clocks, so use an offset of 0.
        offset = otf2::chrono::duration(0);
    }
}
} // namespace time
} // namespace perf
} // namespace lo2s
