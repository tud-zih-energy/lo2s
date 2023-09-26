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
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/types.h>
}

namespace lo2s
{
namespace perf
{

struct __attribute((__packed__)) TracepointSampleType
{
    struct perf_event_header header;
    uint64_t time;
    uint32_t tp_data_size;
};

struct IoReaderIdentity
{
    IoReaderIdentity(tracepoint::EventFormat tracepoint, Cpu cpu) : tracepoint(tracepoint), cpu(cpu)
    {
    }

    tracepoint::EventFormat tracepoint;
    Cpu cpu;

    friend bool operator>(const IoReaderIdentity& lhs, const IoReaderIdentity& rhs)
    {
        if (lhs.cpu == rhs.cpu)
        {
            return lhs.tracepoint > rhs.tracepoint;
        }

        return lhs.cpu > rhs.cpu;
    }

    friend bool operator<(const IoReaderIdentity& lhs, const IoReaderIdentity& rhs)
    {
        if (lhs.cpu == rhs.cpu)
        {
            return lhs.tracepoint < rhs.tracepoint;
        }

        return lhs.cpu < rhs.cpu;
    }
};

class IoReader : public PullReader
{
public:
    using PullReader::fd_;
    IoReader(IoReaderIdentity identity) : identity_(identity)
    {
        struct perf_event_attr attr = common_perf_event_attrs();
        attr.type = PERF_TYPE_TRACEPOINT;

        attr.config = identity.tracepoint.id();

        attr.sample_period = 1;
        attr.sample_type = PERF_SAMPLE_RAW | PERF_SAMPLE_TIME;
        fd_ = perf_event_open(&attr, identity.cpu.as_scope());
        if (!fd_)
        {
            Log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
        }

        Log::debug() << "Opened block_rq_insert_tracing";

        try
        {
            init_mmap();
            Log::debug() << "perf_tracepoint_reader mmap initialized";
        }
        catch (...)
        {
            Log::error() << "Couldn't initialize block:rq_insert reading";
            throw;
        }
    }

    void stop()
    {
        auto ret = fd_.value().ioctl(PERF_EVENT_IOC_DISABLE);
        Log::debug() << "perf_tracepoint_reader ioctl(fd, PERF_EVENT_IOC_DISABLE) = " << ret;
        if (ret == -1)
        {
            throw_errno();
        }
    }

    TracepointSampleType* top()
    {
        return reinterpret_cast<TracepointSampleType*>(get());
    }

    const Fd& fd() const
    {
        assert(fd_);
        return fd_.value();
    }

    IoReader& operator=(const IoReader&) = delete;
    IoReader(const IoReader& other) = delete;

    IoReader& operator=(IoReader&& other)
    {
        PullReader::operator=(std::move(other));
        std::swap(identity_, other.identity_);
        std::swap(fd_, other.fd_);

        return *this;
    }

    IoReader(IoReader&& other) : PullReader(std::move(other)), identity_(other.identity_)
    {
        std::swap(fd_, other.fd_);
    }

private:
    IoReaderIdentity identity_;
};
} // namespace perf
} // namespace lo2s
