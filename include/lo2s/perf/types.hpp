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

#include <deque>
#include <memory>

#include <cstdint>
#include <cstring>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{

template <class T>
class PerfEventCache
{
public:
    PerfEventCache() = delete;
    PerfEventCache(const PerfEventCache&) = delete;
    PerfEventCache& operator=(const PerfEventCache&) = delete;

    PerfEventCache(const T* event, size_t size) : data_(std::make_unique<std::byte[]>(size))
    {
        memcpy(data_.get(), event, size);
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

    struct perf_event_header header;
    uint32_t pid, tid;
    uint64_t addr;
    uint64_t len;
    uint64_t pgoff;
    // Note ISO C++ forbids zero-size array, but this struct is exclusively used as pointer
    char filename[1];
    // struct sample_id id;
};

/**
 * \brief structure for PERF_RECORD_COMM events
 **/

struct __attribute((__packed__)) sample_id
{
    uint32_t pid, tid;
    uint64_t time;
    uint32_t cpu, res;
};

struct __attribute((__packed__)) RecordCommType
{
    struct perf_event_header header;
    uint32_t pid;
    uint32_t tid;
    char comm[1]; // ISO C++ forbits zero-size array
    // struct sample_id id;
};

struct __attribute((__packed__)) RecordMmap2Type
{
    // BAD things happen if you try this
    RecordMmap2Type() = delete;
    RecordMmap2Type(const RecordMmap2Type&) = delete;
    RecordMmap2Type& operator=(const RecordMmap2Type&) = delete;
    RecordMmap2Type(RecordMmap2Type&&) = delete;
    RecordMmap2Type& operator=(RecordMmap2Type&&) = delete;

    struct perf_event_header header;
    uint32_t pid;
    uint32_t tid;
    uint64_t addr;
    uint64_t len;
    uint64_t pgoff;
    uint32_t maj;
    uint32_t min;
    uint64_t ino;
    uint64_t ino_generation;
    uint32_t prot;
    uint32_t flags;
    // Note ISO C++ forbids zero-size array, but this struct is exclusively used as pointer
    char filename[1];
    // struct sample_id sample_id;
};

using RawMemoryMapCache = std::deque<PerfEventCache<RecordMmap2Type>>;
using RawCommCache = std::deque<PerfEventCache<RecordCommType>>;

} // namespace lo2s
