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

#pragma once

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/util.hpp>

#include <stdexcept>

#include <cstdlib>
#include <cstring>

extern "C" {
#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>

#include <linux/perf_event.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
}

namespace lo2s
{
namespace perf
{

static inline int perf_event_paranoid()
{
    try
    {
        return get_sysctl<int>("kernel", "perf_event_paranoid");
    }
    catch (...)
    {
        Log::warn() << "Failed to access kernel.perf_event_paranoid. Assuming 2.";
        return 2;
    }
}

namespace sample
{
template <class T>
class Reader : public EventReader<T>
{
public:
    struct RecordSampleType
    {
        struct perf_event_header header;
        uint64_t ip;
        uint32_t pid, tid;
        uint64_t time;
        /* only relevant for has_cct_ / PERF_SAMPLE_CALLCHAIN */
        uint64_t nr;
        uint64_t ips[1]; // ISO C++ forbits zero-size array
    };

protected:
    Reader(bool has_cct = false) : has_cct_(has_cct)
    {
    }

    using EventReader<T>::init_mmap;
    void init(struct perf_event_attr& perf_attr, pid_t tid, int cpu, bool enable_on_exec,
              size_t mmap_pages)
    {
        Log::debug() << "initializing event_reader for tid: " << tid
                     << ", enable_on_exec: " << enable_on_exec;

        // We need this to get all mmap_events
        if (enable_on_exec)
        {
            perf_attr.enable_on_exec = 1;
        }

        // TODO see if we can remove remove tid
        perf_attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME;
        if (has_cct_)
        {
            perf_attr.sample_type |= PERF_SAMPLE_CALLCHAIN;
        }

        // we'll enable it later otherwise mmap would not work
        perf_attr.disabled = 1;

        perf_attr.precise_ip = 3; // request exact address
        /* precise_ip is an unsigned integer therefore we have to check if we get an underflow
         * and the value of it is greater than the initial value */
        do
        {
            fd_ = syscall(__NR_perf_event_open, &perf_attr, tid, cpu, -1, 0);

            if (errno == EACCES && !perf_attr.exclude_kernel && perf_event_paranoid() > 1)
            {
                perf_attr.exclude_kernel = 1;
                Log::warn()
                    << "kernel.perf_event_paranoid>1, retrying with excluding kernel samples.";
                continue;
            }

            /* reduce exactness of IP can help if the kernel does not support really exact events */
            if (perf_attr.precise_ip == 0)
                break;
            else
                perf_attr.precise_ip--;
        } while (fd_ <= 0);

        if (fd_ < 0)
        {
            // TODO if there is a EACCESS, we should retry without the kernel flag!
            // Test if it then works with paranoid=2
            Log::error() << "perf_event_open for sampling failed";
            throw_errno();
        }
        Log::debug() << "Using precise_ip level: " << perf_attr.precise_ip;

        // Exception safe, so much wow!
        try
        {
            // asynchronous delivery
            // if (fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK))
            if (fcntl(fd_, F_SETFL, O_NONBLOCK))
            {
                throw_errno();
            }

            init_mmap(fd_, mmap_pages);
            Log::debug() << "mmap initialized";

            if (!enable_on_exec)
            {
                auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
                Log::debug() << "ioctl(fd, PERF_EVENT_IOC_ENABLE) = " << ret;
                if (ret == -1)
                {
                    throw_errno();
                }
            }
        }
        catch (...)
        {
            close(fd_);
            throw;
        }
    }

    Reader(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader& operator=(Reader&&) = delete;

    ~Reader()
    {
        if ((this->total_samples > 0) &&
            ((this->throttle_samples * 100) / this->total_samples) > 20)
        {
            Log::warn() << "The given sample period is too low and the kernel assumes it has an "
                           "overwhelming influence on the runtime!\n"
                           "Thus, the kernel increased and decreased this period often "
                           "(> 20 % of the throttling events, < 80 % sampling events)."
                           "You can increase /proc/sys/kernel/perf_event_max_sample_rate to some "
                           "extend to prevent the kernel from doing that"
                           " or you can increase your sampling period.";
        }
        close(fd_);
    }

protected:
    bool has_cct_;

private:
    int fd_ = -1;
};
}
}
}
