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
 
 #include <lo2s/perf/event_reader.hpp>

namespace lo2s
{
namespace perf
{
namespace context_switch
{
template <class T>
class Reader : public EventReader<T>
{
protected:
    using EventReader<T>::init_mmap;
    Reader(int cpuid)
    {
        struct perf_event_attr perf_attr;
        memset(&attr, 0, sizeof(struct perf_event_attr));
        perf_attr.type = PERF_TYPE_SOFTWARE;
        perf_attr.size = sizeof(struct perf_event_attr);
        perf_attr.config = PERF_COUNT_SW_DUMMY;
        perf_attr.disable = 1;
        perf_attr.sample_period = 1;
        perf_attr.sample_type = PERF_SAMPLE_TIME;
#if !defined(HW_BREAKPOINT_COMPAT) && defined(USE_PERF_CLOCKID)
        perf_attr.use_clockid = config().use_clockid;
        perf_attr.clockid = config().clockid;
#endif
        perf_attr.context_switch = 1;
        
        fd_ = perf_event_open(&perf_attr, -1, cpu, -1, 0);
        if(fd_ < 0)
        {
            Log::error() << "perf_event_open for context switches failed.";
        }
        Log::debug() << "Opened perf_context_switch_reader for cpu " << cpu_;

        try
        {
            //asynchronous delivery
            if(fcntl(fd_, F_SETFL, O_NONBLOCK))
            {
                throw_errno();
            }

            init_mmap(fd_, config().mmap_pages);
            Log::debug() << "perf_context_switch_reader mmap initialized";

            if(ioctl(fd_, PERF_EVENT_IOC_ENAVBLE) == -1)
            {
                throw_errno();
            }
        }
        catch (...)
        {
            close(fd_);
            throw;
        }
    }

    ~Reader()
    {
        if(fd_ != -1)
        {
            close(fd_);
        }
    }
private:
    int fd_;
}
}
}
}
