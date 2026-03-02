// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/log.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/types/cpu.hpp>

#include <utility>

#include <cstdint>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf
{

struct __attribute((__packed__)) TracepointSampleType
{
    struct perf_event_header header;
    uint64_t time;
    uint32_t tp_data_size;
};

struct IoReaderIdentity
{
    IoReaderIdentity(tracepoint::TracepointEventAttr event, Cpu cpu)
    : tracepoint_(std::move(event)), cpu(cpu)
    {
    }

    tracepoint::TracepointEventAttr tracepoint_;
    Cpu cpu;

    tracepoint::TracepointEventAttr tracepoint() const
    {
        return tracepoint_;
    }

    friend bool operator>(const IoReaderIdentity& lhs, const IoReaderIdentity& rhs)
    {
        if (lhs.cpu == rhs.cpu)
        {
            return lhs.tracepoint_ > rhs.tracepoint_;
        }

        return lhs.cpu > rhs.cpu;
    }

    friend bool operator==(const IoReaderIdentity& lhs, const IoReaderIdentity& rhs)
    {
        return lhs.cpu == rhs.cpu && lhs.tracepoint_ == rhs.tracepoint_;
    }

    friend bool operator<(const IoReaderIdentity& lhs, const IoReaderIdentity& rhs)
    {
        if (lhs.cpu == rhs.cpu)
        {
            return lhs.tracepoint_ < rhs.tracepoint_;
        }

        return lhs.cpu < rhs.cpu;
    }
};

class IoReader : public PullReader
{
public:
    IoReader(IoReaderIdentity identity)
    : identity_(identity), event_(identity_.tracepoint().open(identity.cpu))
    {
        Log::debug() << "Opened block_rq_insert_tracing";

        try
        {
            init_mmap(event_.get_fd());
            Log::debug() << "perf_tracepoint_reader mmap initialized";

            event_.enable();
        }
        catch (...)
        {
            Log::error() << "Couldn't initialize block:rq_insert reading";
            throw;
        }
    }

    void stop()
    {
        event_.disable();
    }

    TracepointSampleType* top()
    {
        return reinterpret_cast<TracepointSampleType*>(get());
    }

    int fd() const
    {
        return event_.get_fd();
    }

    IoReader& operator=(const IoReader&) = delete;
    IoReader(const IoReader& other) = delete;

    IoReader& operator=(IoReader&& other) noexcept
    {
        std::swap(identity_, other.identity_);
        std::swap(event_, other.event_);
        PullReader::operator=(std::move(other));

        return *this;
    }

    IoReader(IoReader&& other) noexcept
    : PullReader(std::move(other)),          // NOLINT
      identity_(std::move(other.identity_)), // NOLINT
      event_(std::move(other.event_))        // NOLINT
    {
    }

    ~IoReader() = default;

private:
    IoReaderIdentity identity_;
    EventGuard event_;
};
} // namespace lo2s::perf
