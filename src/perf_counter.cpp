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

#include <lo2s/perf_counter.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>

#include <cinttypes>
#include <cstring>

extern "C" {
#include <syscall.h>
#include <unistd.h>

#include <linux/perf_event.h>
}

namespace lo2s
{
perf_counter::perf_counter(pid_t tid, perf_type_id type, uint64_t config, uint64_t config1)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.sample_period = 0;
    attr.type = type;
    attr.config = config;
    attr.config1 = config1;
    // Needed when scaling multiplexed events, and recognize activation phases
    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    fd_ = syscall(__NR_perf_event_open, &attr, tid, -1, -1, 0);
    if (fd_ < 0)
    {
        log::error() << "perf_event_open for counter failed";
        throw_errno();
    }
}

double perf_counter::read()
{
    ver infos;
    auto res = ::read(fd_, &infos, sizeof(infos));
    if (res != sizeof(infos))
    {
        log::error() << "could not read counter values";
        throw_errno();
    }
    double value = (infos - previous_).scale() + accumulated_;
    previous_ = infos;
    accumulated_ = value;
    return accumulated_;
}
}
