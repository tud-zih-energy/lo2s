// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <deque>
#include <memory>
#include <utility>

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf
{

template <class T>
class PerfEventCache
{
public:
    PerfEventCache() = delete;
    PerfEventCache(const PerfEventCache&) = delete;
    PerfEventCache& operator=(const PerfEventCache&) = delete;
    ~PerfEventCache() = default;

    PerfEventCache(PerfEventCache&& other) noexcept
    {
        std::swap(data_, other.data_);
    }

    PerfEventCache& operator=(PerfEventCache&& other) noexcept
    {
        std::swap(data_, other.data_);
        return *this;
    }

    PerfEventCache(const T* event) : data_(std::make_unique<std::byte[]>(event->header.size))
    {
        memcpy(data_.get(), event, event->header.size);
    }

    T* get()
    {
        return reinterpret_cast<T*>(data_.get());
    }

    const T* get() const
    {
        return reinterpret_cast<T*>(data_.get());
    }

private:
    std::unique_ptr<std::byte[]> data_;
};

struct RecordMmapType
{
    // BAD things happen if you try this
    RecordMmapType() = delete;
    RecordMmapType(const RecordMmapType&) = delete;
    RecordMmapType& operator=(const RecordMmapType&) = delete;
    RecordMmapType(RecordMmapType&&) = delete;
    RecordMmapType& operator=(RecordMmapType&&) = delete;
    ~RecordMmapType() = default;

    struct perf_event_header header;
    uint32_t pid, tid;
    uint64_t addr;
    uint64_t len;
    uint64_t pgoff;
    // Note ISO C++ forbids zero-size array, but this struct is exclusively used as pointer
    char filename[1];
    // struct sample_id id;
};

struct sample_id
{
    uint32_t pid, tid;
    uint64_t time;
    uint32_t cpu, res;
};

using RawMemoryMapCache = std::deque<PerfEventCache<RecordMmapType>>;

} // namespace lo2s::perf
