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
enum class BioEventType
{
    INSERT,
    ISSUE,
    COMPLETE
};

struct __attribute((__packed__)) RecordBlock
{
    uint16_t common_type; // common fields included in all tracepoints, ignored here
    uint8_t common_flag;
    uint8_t common_preempt_count;
    int32_t common_pid;

    uint32_t dev;    // the accessed device (as dev_t)
    char padding[4]; // padding because of struct alignment
    uint64_t sector; // the accessed sector on the device

    uint32_t nr_sector;     // the number of sectors written
    int32_t error_or_bytes; // for insert/issue: number of bytes written (nr_sector * 512)
                            // for complete: the error code of the operation

    char rwbs[8]; // the type of the operation. "R" for read, "W" for write, etc.
};

struct RecordBlockSampleType
{
    struct perf_event_header header;
    uint64_t time;
    uint32_t tp_data_size;
    RecordBlock blk;
};

class Reader : public PullReader
{
public:
    struct IdentityType
    {
        IdentityType(BioEventType type, Cpu cpu) : type(type), cpu(cpu)
        {
        }

        BioEventType type;
        Cpu cpu;

        friend bool operator<(const IdentityType& lhs, const IdentityType& rhs)
        {
            if (lhs.cpu == rhs.cpu)
            {
                return lhs.type < rhs.type;
            }

            return lhs.cpu < rhs.cpu;
        }
    };

    Reader(IdentityType identity) : type_(identity.type), cpu_(identity.cpu)
    {
        struct perf_event_attr attr = common_perf_event_attrs();
        attr.type = PERF_TYPE_TRACEPOINT;
        if (type_ == BioEventType::INSERT)
        {
            attr.config = tracepoint::EventFormat("block:block_rq_insert").id();
        }
        else if (type_ == BioEventType::ISSUE)
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
    }

    RecordBlockSampleType* top()
    {
        return reinterpret_cast<RecordBlockSampleType*>(get());
    }

    int fd() const
    {
        return fd_;
    }

    Reader& operator=(const Reader&) = delete;
    Reader(const Reader& other) = delete;

    Reader& operator=(Reader&& other)
    {
        PullReader::operator=(std::move(other));
        std::swap(cpu_, other.cpu_);
        std::swap(fd_, other.fd_);
        std::swap(type_, other.type_);

        return *this;
    }

    Reader(Reader&& other) : PullReader(std::move(other)), type_(other.type_), cpu_(other.cpu_)
    {
        std::swap(fd_, other.fd_);
    }

protected:
    BioEventType type_;

private:
    Cpu cpu_;
    int fd_ = -1;
};
} // namespace bio
} // namespace perf
} // namespace lo2s
