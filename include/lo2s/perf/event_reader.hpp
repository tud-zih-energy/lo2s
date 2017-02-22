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
#include <lo2s/platform.hpp>
#include <lo2s/util.hpp>

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstring>

extern "C" {
#include <linux/perf_event.h>
#include <sys/mman.h>
}

/* perf sample has 16 bits size limit */
#define PERF_SAMPLE_MAX_SIZE (1 << 16)

namespace lo2s
{
namespace perf
{
template <class T>
class EventReader
{
protected:
    using CRTP = T;

public:
    // We allow the subclass to override the actual types because the sample type depends on the
    // attr settings
    using RecordSampleType = perf_event_header;

    // We don't need the type of the subclass here, because these types are static
    using RecordUnknownType = perf_event_header;

    struct RecordMmapType
    {
        struct perf_event_header header;
        uint32_t pid, tid;
        uint64_t addr;
        uint64_t len;
        uint64_t pgoff;
        // Note ISO C++ forbids zero-size array, but this struct is exclusively used as pointer
        char filename[1];
        // struct sample_id sample_id;
    };
    struct RecordMmap2Type
    {
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
    struct RecordLostType
    {
        struct perf_event_header header;
        uint64_t id;
        uint64_t lost;
        // struct sample_id		sample_id;
    };
    struct RecordForkType
    {
        struct perf_event_header header;
        uint32_t pid;
        uint32_t ppid;
        uint32_t tid;
        uint32_t ptid;
        uint64_t time;
        // struct sample_id sample_id;
    };

    ~EventReader()
    {
        if (lost_samples > 0)
        {
            Log::warn() << "Lost a total of " << lost_samples << " samples in event_reader<"
                        << typeid(CRTP).name() << ">.";
        }
    }

protected:
    void init_mmap(int fd, size_t mmap_pages)
    {
        assert(mmap_pages > 0);
        base = mmap(NULL, (mmap_pages + 1) * get_page_size(), PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0);
        assert(mmap_pages_ == 0);
        mmap_pages_ = mmap_pages;
        // Should not be necessary to check for nullptr, but we've seen it!
        if (base == MAP_FAILED || base == nullptr)
        {
            Log::error() << "mmap failed. You can decrease the buffer size or try to increase "
                            "/proc/sys/kernel/perf_event_mlock_kb";
            throw_errno();
        }
    }

public:
    void read()
    {
        auto cur_head = data_head();
        auto cur_tail = data_tail();

        Log::trace() << "head: " << cur_head << ", tail: " << cur_tail;

        /* if the ring buffer has been filled to fast, we skip the current entries*/
        auto diff = cur_head - cur_tail;
        assert(cur_tail <= cur_head);
        if (diff > data_size())
        {
            Log::error() << "perf ring buffer overflow. "
                            "The trace is missing events. "
                            "Increase the buffer size or the sampling period";
        }
        else
        {
            auto d = data();
            int64_t read_samples = 0;
            while (cur_tail < cur_head)
            {
                auto index = cur_tail % data_size();
                auto event_header_p = (struct perf_event_header*)(d + index);
                auto len = event_header_p->size;
                if (cur_tail + len > cur_head)
                {
                    Log::warn() << "perf_event goes beyond tail. skipping.";
                    break;
                }
                read_samples++;
                total_samples++;
                // Event spans the wrap-around of the ring buffer
                if (index + len > data_size())
                {
                    char* dst = event_copy;
                    do
                    {
                        auto cpy = std::min<std::size_t>(data_size() - index, len);
                        memcpy(dst, d + index, cpy);
                        index = (index + cpy) % data_size();
                        dst += cpy;
                        len -= cpy;
                    } while (len);
                    event_header_p = (struct perf_event_header*)(event_copy);
                }

                bool stop = false;
                auto crtp_this = static_cast<CRTP*>(this);
                switch (event_header_p->type)
                {
                case PERF_RECORD_MMAP:
                    stop = crtp_this->handle((const RecordMmapType*)event_header_p);
                    break;
                case PERF_RECORD_MMAP2:
                    stop = crtp_this->handle((const RecordMmap2Type*)event_header_p);
                    break;
                case PERF_RECORD_EXIT:
                    Log::debug() << "encountered exit event.";
                    break;
                case PERF_RECORD_THROTTLE: /* fall-through */
                case PERF_RECORD_UNTHROTTLE:
                    throttle_samples++;
                    break;
                case PERF_RECORD_LOST:
                {
                    auto lost = (const RecordLostType*)event_header_p;
                    lost_samples += lost->lost;
                    Log::info() << "Lost " << lost->lost << " samples during this chunk.";
                    break;
                }
                case PERF_RECORD_FORK:
                    stop = crtp_this->handle((const RecordForkType*)event_header_p);
                    break;
                case PERF_RECORD_SAMPLE:
                {
                    // Use CRTP here because the struct type depends on the perf attr
                    using ActualSampleType = typename CRTP::RecordSampleType;
                    stop = crtp_this->handle((const ActualSampleType*)event_header_p);
                    break;
                }
                default:
                    stop = crtp_this->handle((const RecordUnknownType*)event_header_p);
                }
                cur_tail += event_header_p->size;
                if (stop)
                {
                    break;
                }
            }
            diff = data_head() - cur_tail;
            if (diff > data_size())
            {
                Log::error() << "perf ring buffer overflow occurred while reading. "
                                "The trace may be garbage. "
                                "Increase the buffer size or the sampling period";
            }
            Log::trace() << "read " << read_samples << " samples.";
        }
        data_tail(cur_tail);
    }

private:
    const struct perf_event_mmap_page* header() const
    {
        return (const struct perf_event_mmap_page*)base;
    }

    struct perf_event_mmap_page* header()
    {
        return (struct perf_event_mmap_page*)base;
    }

    uint64_t data_head() const
    {
        auto head = header()->data_head;
        return head;
    }

    void data_tail(uint64_t tail)
    {
        /* ensure all reads are done before we write the tail out. */
        rmb();
        header()->data_tail = tail;
    }

    uint64_t data_tail() const
    {
        auto tail = header()->data_tail;
        // TODO do we need a rmb here?
        return tail;
    }

    uint64_t data_size() const
    {
        // workaround for old kernels
        // assert(header()->data_size == mmap_pages_ * get_page_size());
        return mmap_pages_ * get_page_size();
    }

    char* data()
    {
        // workaround for old kernels
        // assert(header()->data_offset == get_page_size());
        return (char*)base + get_page_size();
    }

public:
    template <class UNKNOWN_RECORD_TYPE>
    bool handle(const UNKNOWN_RECORD_TYPE* record)
    {
        auto header = (perf_event_header*)record;
        Log::warn() << "unknown perf record type: " << header->type;
        return false;
    }

protected:
    int64_t total_samples = 0;
    int64_t throttle_samples = 0;
    int64_t lost_samples = 0;
    size_t mmap_pages_ = 0;

private:
    void* base;
    char event_copy[PERF_SAMPLE_MAX_SIZE] __attribute__((aligned(8)));
};
}
}
