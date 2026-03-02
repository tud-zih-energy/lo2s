// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/perf/time/converter.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/time/reader.hpp>

#include <otf2xx/chrono/duration.hpp>

#include <chrono>
#include <exception>
#include <ios>

namespace lo2s::perf::time
{
Converter::Converter() : offset(otf2::chrono::duration(0))
{
    try
    {
        Reader reader;
        reader.read();
        if (reader.perf_time.time_since_epoch().count() == 0)
        {
            Log::error() << "Could not determine perf_time offset. Synchronization event was "
                            "not triggered.";
            return;
        }

        // we expect local_time <= perf_time, i.e. time_diff < 0
        const auto time_diff =
            reader.local_time.time_since_epoch() - reader.perf_time.time_since_epoch();

        if (lo2s::config().perf.clockid.has_value())
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
    catch (std::exception& e)
    {
        Log::warn() << "Can not create time synchronization perf event: " << e.what();
        Log::warn()
            << "Assuming time synchronization offset of 0, your timestamps might be imprecise";
        return;
    }
}
} // namespace lo2s::perf::time
