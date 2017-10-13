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

#include <lo2s/metric/perf_counter.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/util.hpp>

#include <cstdint>
#include <cstring>

extern "C" {
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#include <linux/perf_event.h>
}

namespace
{
static int perf_try_event_open(struct perf_event_attr* perf_attr, pid_t tid, int cpu, int group_fd,
                               unsigned long flags)
{
    int fd = lo2s::perf::perf_event_open(perf_attr, tid, cpu, group_fd, flags);
    if (fd < 0 && errno == EACCES && !perf_attr->exclude_kernel &&
        lo2s::perf::perf_event_paranoid() > 1)
    {
        perf_attr->exclude_kernel = 1;
        lo2s::Log::warn() << "kernel.perf_event_paranoid > 1, retrying without kernel samples:";
        lo2s::Log::warn() << " * sysctl kernel.perf_event_paranoid=1";
        lo2s::Log::warn() << " * run lo2s as root";
        lo2s::Log::warn() << " * run with --no-kernel to disable kernel space monitoring in "
                             "the first place,";
        fd = lo2s::perf::perf_event_open(perf_attr, tid, cpu, group_fd, flags);
    }
    return fd;
}
}

namespace lo2s
{
namespace metric
{
PerfCounter::PerfCounter(pid_t tid, perf_type_id type, std::uint64_t config, std::uint64_t config1,
                         int group_fd)
{
    struct perf_event_attr perf_attr;
    memset(&perf_attr, 0, sizeof(perf_attr));
    perf_attr.size = sizeof(perf_attr);
    perf_attr.sample_period = 0;
    perf_attr.type = type;
    perf_attr.config = config;
    perf_attr.config1 = config1;
    perf_attr.exclude_kernel = lo2s::config().exclude_kernel;
    // Needed when scaling multiplexed events, and recognize activation phases
    perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    fd_ = perf_try_event_open(&perf_attr, tid, -1, group_fd, 0);
    if (fd_ < 0)
    {
        Log::error() << "perf_event_open for counter failed";
        throw_errno();
    }
}

double PerfCounter::read()
{
    Ver infos;
    auto res = ::read(fd_, &infos, sizeof(infos));
    if (res != sizeof(infos))
    {
        Log::error() << "could not read counter values";
        throw_errno();
    }
    double value = (infos - previous_).scale() + accumulated_;
    previous_ = infos;
    accumulated_ = value;
    return accumulated_;
}
}
}
