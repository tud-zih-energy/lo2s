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
#include <lo2s/perf/reader.hpp>
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
    IoReader(IoReaderIdentity identity) : identity_(identity)
    {
        perf::TracepointEvent event(0, identity.tracepoint.id());

        try
        {
            ev_instance_ = event.open(identity.cpu);
        }
        catch (const std::system_error& e)
        {
            Log::error() << "perf_event_open for raw tracepoint failed.";
            throw_errno();
        }

        Log::debug() << "Opened block_rq_insert_tracing";

        try
        {
            init_mmap(ev_instance_.get_fd());
            Log::debug() << "perf_tracepoint_reader mmap initialized";

            ev_instance_.enable();
        }
        catch (...)
        {
            Log::error() << "Couldn't initialize block:rq_insert reading";
            throw;
        }
    }

    void stop()
    {
        ev_instance_.disable();
    }

    TracepointSampleType* top()
    {
        return reinterpret_cast<TracepointSampleType*>(get());
    }

    int fd() const
    {
        return ev_instance_.get_fd();
    }

    IoReader& operator=(const IoReader&) = delete;
    IoReader(const IoReader& other) = delete;

    IoReader& operator=(IoReader&& other)
    {
        PullReader::operator=(std::move(other));
        std::swap(identity_, other.identity_);
        std::swap(ev_instance_, other.ev_instance_);

        return *this;
    }

    IoReader(IoReader&& other) : PullReader(std::move(other)), identity_(other.identity_)
    {
        std::swap(ev_instance_, other.ev_instance_);
    }

private:
    IoReaderIdentity identity_;
    perf::PerfEventInstance ev_instance_;
};
} // namespace perf
} // namespace lo2s
