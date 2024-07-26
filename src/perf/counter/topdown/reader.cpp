/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2021,
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

#include <linux/perf_event.h>
#include <lo2s/error.hpp>
#include <lo2s/perf/counter/topdown/reader.hpp>
#include <lo2s/perf/counter/topdown/writer.hpp>
#include <lo2s/perf/util.hpp>
#include <lo2s/time/time.hpp>

#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/util.hpp>

#include <cstdlib>

extern "C"
{
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <unistd.h>
}
namespace lo2s
{
namespace perf
{
namespace counter
{
namespace topdown
{

template <class T>
Reader<T>::Reader(ExecutionScope scope)
{
    struct itimerspec tspec;
    memset(&tspec, 0, sizeof(struct itimerspec));
    tspec.it_value.tv_nsec = 1;

    tspec.it_interval.tv_sec =
        std::chrono::duration_cast<std::chrono::seconds>(config().userspace_read_interval).count();

    tspec.it_interval.tv_nsec =
        (config().userspace_read_interval % std::chrono::seconds(1)).count();
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    timerfd_settime(timer_fd_, TFD_TIMER_ABSTIME, &tspec, NULL);

    struct perf_event_attr leader_perf_attr;
    memset(&leader_perf_attr, 0, sizeof(struct perf_event_attr));
    leader_perf_attr.type = PERF_TYPE_RAW;
    leader_perf_attr.size = sizeof(struct perf_event_attr);
    leader_perf_attr.config = 0x400;
    leader_perf_attr.read_format = PERF_FORMAT_GROUP;
    leader_perf_attr.disabled = 1;

    leader_fd_ = perf_event_open(&leader_perf_attr, scope, -1, 0);

    if (leader_fd_ == -1)
    {
        Log::error() << errno;
    }
    std::vector<uint64_t> counters = { 0x8000, 0x8100, 0x8200, 0x8300 };

    for (auto counter : counters)
    {
        struct perf_event_attr perf_attr;
        memset(&perf_attr, 0, sizeof(perf_attr));
        perf_attr.type = PERF_TYPE_RAW, perf_attr.size = sizeof(struct perf_event_attr);
        perf_attr.config = counter;
        perf_attr.read_format = PERF_FORMAT_GROUP;
        perf_attr.disabled = 0;
        int fd = perf_event_open(&perf_attr, scope, leader_fd_, 0);

        if (fd == -1)
        {
            Log::error() << errno;
        }
        counter_fds_.emplace_back(fd);
    }

    ::ioctl(leader_fd_, PERF_EVENT_IOC_ENABLE);
}

template <class T>
void Reader<T>::read()
{
    struct TopdownEvent ev;
    [[maybe_unused]] auto ret = ::read(leader_fd_, &ev, sizeof(ev));

    assert(ret == sizeof(ev));

    static_cast<T*>(this)->handle(&ev);

    [[maybe_unused]] uint64_t expirations;
    if (::read(timer_fd_, &expirations, sizeof(expirations)) == -1 && errno != EAGAIN)
    {
        Log::error() << "Flushing timer fd failed";
        throw_errno();
    }
}

template class Reader<Writer>;
} // namespace topdown
} // namespace counter
} // namespace perf
} // namespace lo2s
