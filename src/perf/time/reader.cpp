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
    struct perf_event_attr attr = common_perf_event_attrs();

    attr.sample_type = PERF_SAMPLE_TIME;
    attr.exclude_kernel = 1;

#ifndef USE_HW_BREAKPOINT_COMPAT
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.bp_type = HW_BREAKPOINT_W;
    attr.bp_addr = (uint64_t)(&local_time);
    attr.bp_len = HW_BREAKPOINT_LEN_8;
    attr.wakeup_events = 1;
    attr.sample_period = 1;
#else
    attr.type = PERF_TYPE_HARDWARE;
    attr.config = PERF_COUNT_HW_INSTRUCTIONS;
    attr.sample_period = 100000000;
    attr.task = 1;
#endif

    try
    {
        fd_ = perf_event_open(&attr, ExecutionScope(Thread(0)), -1, 0);
        if (fd_ == -1)
        {
            throw_errno();
        }

        init_mmap(fd_);

        if (ioctl(fd_, PERF_EVENT_IOC_ENABLE) == -1)
        {
            throw_errno();
        }
    }
    catch (...)
    {
#ifndef USE_HW_BREAKPOINT_COMPAT
        Log::error() << "time synchronization with hardware breakpoints failed, try rebuilding lo2s with -DUSE_HW_BREAKPOINT_COMPAT=ON";
#else
        Log::error() << "opening the perf event for HW_BREAKPOINT_COMPAT time synchronization failed";
#endif
        close(fd_);
        throw;
    }

#ifdef USE_HW_BREAKPOINT_COMPAT
    auto pid = fork();
    if (pid == 0)
    {
        abort();
    }
    else if (pid == -1)
    {
        close(fd_);
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
Reader::~Reader()
{
    close(fd_);
}

} // namespace time
} // namespace perf
} // namespace lo2s
