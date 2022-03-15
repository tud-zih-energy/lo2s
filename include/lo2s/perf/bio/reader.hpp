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

#pragma once

#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/util.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

#include <filesystem>

#include <ios>

#include <cstddef>

extern "C"
{
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
}

namespace lo2s
{
namespace perf
{
namespace bio
{

template <class T>
class Reader : public EventReader<T>
{
public:
    struct RecordSampleType
    {
        struct perf_event_header header;
        uint64_t time;
        uint32_t tp_data_size;
        char tp_data[1];
    };

    Reader(Cpu cpu, BioEventType type) : type_(type), cpu_(cpu)
    {
        struct perf_event_attr attr = common_perf_event_attrs();
        attr.type = PERF_TYPE_TRACEPOINT;
        if (type == BioEventType::INSERT)
        {
            attr.config = tracepoint::EventFormat("block:block_rq_insert").id();
        }
        else if (type == BioEventType::ISSUE)
        {
            attr.config = tracepoint::EventFormat("block:block_rq_issue").id();
        }
        else
        {
            attr.config = tracepoint::EventFormat("block:block_rq_complete").id();
        }

        attr.sample_period = 1;
        attr.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME;

        fd_ = perf_event_open(&attr, cpu_.as_scope(), -1, 0);
        if (fd_ < 0)
        {
            Log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
        }

        Log::debug() << "Opened block_rq_insert_tracing";

        try
        {
            if (fcntl(fd_, F_SETFL, O_NONBLOCK))
            {
                throw_errno();
            }

            init_mmap(fd_);
            Log::debug() << "perf_tracepoint_reader mmap initialized";

            auto ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE);
            Log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_ENABLE) = " << ret;
            if (ret == -1)
            {
                throw_errno();
            }
        }
        catch (...)
        {
            Log::error() << "Couldn't initialize block:rq_insert reading";
            close(fd_);
            throw;
        }
    }

    Reader(Reader&& other)
    : EventReader<T>(std::forward<perf::EventReader<T>>(other)), cpu_(other.cpu_)
    {
        std::swap(fd_, other.fd_);
    }

    ~Reader()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

    void stop()
    {
        auto ret = ioctl(fd_, PERF_EVENT_IOC_DISABLE);
        Log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_DISABLE) = " << ret;
        if (ret == -1)
        {
            throw_errno();
        }
        this->read();
    }

protected:
    using EventReader<T>::init_mmap;
    BioEventType type_;

private:
    Cpu cpu_;
    int fd_ = -1;
};
} // namespace bio
} // namespace perf
} // namespace lo2s
