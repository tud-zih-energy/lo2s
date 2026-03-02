// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/perf/time/reader.hpp>

#include <lo2s/log.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_composer.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/types/thread.hpp>

#include <otf2xx/chrono/time_point.hpp>

#include <cstdint>

#ifndef USE_HW_BREAKPOINT_COMPAT
extern "C"
{
}
#else
extern "C"
{
#include <sys/types.h>
#include <sys/wait.h>
}
#endif

namespace lo2s::perf::time
{
EventGuard Reader::create_time_event(otf2::chrono::time_point& local_time)
{
    static_assert(sizeof(local_time) == sizeof(uint64_t),
                  "The local time object must not be a big fat "
                  "object, or the hardware breakpoint won't work.");

    try
    {
        EventAttr event =
            EventComposer::instance().create_time_event(reinterpret_cast<uint64_t>(&local_time));
        return event.open(Thread(0));
    }
    catch (EventAttr::InvalidEvent& e)
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
}

Reader::Reader() : ev_instance_(Reader::create_time_event(local_time))
{

    init_mmap(ev_instance_.get_fd());
    ev_instance_.enable();

#ifdef USE_HW_BREAKPOINT_COMPAT
    auto pid = fork();
    if (pid == 0)
    {
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

} // namespace lo2s::perf::time
