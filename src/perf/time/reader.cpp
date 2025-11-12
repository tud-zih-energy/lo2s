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

#include <lo2s/perf/time/reader.hpp>

#include <lo2s/log.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/time/time.hpp>

#include <otf2xx/chrono/chrono.hpp>

#ifndef USE_HW_BREAKPOINT_COMPAT
extern "C"
{
#include <linux/hw_breakpoint.h>
}
#else
extern "C"
{
#include <sys/types.h>
#include <sys/wait.h>
}
#endif

extern "C"
{
#include <sys/ioctl.h>
}

namespace lo2s
{
namespace perf
{
namespace time
{
Reader::Reader()
{
    static_assert(sizeof(local_time) == 8, "The local time object must not be a big fat "
                                           "object, or the hardware breakpoint won't work.");

    EventAttr event =
        EventComposer::instance().create_time_event(reinterpret_cast<uint64_t>(&local_time));

    try
    {
        ev_instance_ = event.open(Thread(0).as_scope()).unpack_ok();

        init_mmap(ev_instance_.value().get_weak_fd());
        ev_instance_.value().enable();
    }
    catch (...)
    {
#ifndef USE_HW_BREAKPOINT_COMPAT
        Log::error() << "time synchronization with hardware breakpoints failed, try rebuilding "
                        "lo2s with -DUSE_HW_BREAKPOINT_COMPAT=ON";
#else
        Log::error()
            << "opening the perf event for HW_BREAKPOINT_COMPAT time synchronization failed";
#endif
        throw;
    }

#ifdef USE_HW_BREAKPOINT_COMPAT
    auto pid = fork();
    if (pid == 0)
    {cg
        abort();
    }
    else if (pid == -1)
    {
        throw_errno();
    }
    waitpid(pid, NULL, 0);
#endif
    local_time = lo2s::time::now();
}

bool Reader::handle(const RecordSyncType* sync_event)
{
    Log::trace() << "time_reader::handle called";
    perf_time = convert_time_point(sync_event->time);
    return true;
}

} // namespace time
} // namespace perf
} // namespace lo2s
