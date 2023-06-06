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

#include <lo2s/build_config.hpp>
#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/mmap.hpp>
#include <lo2s/platform.hpp>
#include <lo2s/util.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C"
{
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

    using RecordMmapType = lo2s::RecordMmapType;

    struct RecordMmap2Type
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
    struct RecordLostType
    {
        struct perf_event_header header;
        uint64_t id;
        uint64_t lost;
        // struct sample_id		sample_id;
    };

    struct RecordLostSamplesType
    {
        struct perf_event_header header;
        uint64_t lost;
        // struct sample_id      sample_id;
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
    struct RecordSwitchType
    {
        struct perf_event_header header;
        uint32_t pid, tid;
        uint64_t time;
        uint32_t cpu, res;
        // struct sample_id sample_id;
    };
    struct RecordSwitchCpuWideType
    {
        struct perf_event_header header;
        uint32_t next_prev_pid;
        uint32_t next_prev_tid;
        uint32_t pid, tid;
        uint64_t time;
        // struct sample_id sample_id;
    };

    /**
     * \brief structure for PERF_RECORD_COMM events
     **/
    struct RecordCommType
    {
        struct perf_event_header header;
        uint32_t pid;
        uint32_t tid;
        char comm[1]; // ISO C++ forbits zero-size array
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
    void init_mmap(int fd)
    {
        fd_ = fd;

        mmap_pages_ = config().mmap_pages;

        base = mmap(NULL, (mmap_pages_ + 1) * get_page_size(), PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0);
        // Should not be necessary to check for nullptr, but we've seen it!
        if (base == MAP_FAILED || base == nullptr)
        {
            Log::error() << "mapping memory for recording events failed. You can try "
                            "to decrease the buffer size with the -m flag, or try to increase "
                            "the amount of mappable memory by increasing /proc/sys/kernel/"
                            "perf_event_mlock_kb";
            throw_errno();
        }
    }

public:
    void read()
    {
        int64_t read_samples = 0;
        while (!empty())
        {
            auto event_header_p = get();
            read_samples++;
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
            case PERF_RECORD_SWITCH:
                stop = crtp_this->handle((const RecordSwitchType*)event_header_p);
                break;
            case PERF_RECORD_SWITCH_CPU_WIDE:
                stop = crtp_this->handle((const RecordSwitchCpuWideType*)event_header_p);
                break;
            case PERF_RECORD_THROTTLE: /* fall-through */
            case PERF_RECORD_UNTHROTTLE:
                throttle_samples++;
                break;
            case PERF_RECORD_LOST:
            {
                auto lost = (const RecordLostType*)event_header_p;
                lost_samples += lost->lost;
                Log::warn() << "Lost " << lost->lost << " samples during this chunk.";
                break;
            }
#ifdef HAVE_PERF_RECORD_LOST_SAMPLES
            case PERF_RECORD_LOST_SAMPLES:
            {
                auto lost = (const RecordLostSamplesType*)event_header_p;
                lost_samples += lost->lost;
                Log::warn() << "Lost " << lost->lost << " samples during this chunk.";
                break;
            }
#endif
            case PERF_RECORD_EXIT:
                // We might get those as a side effect of time synchronization,
                // when using HW_BREAKPOINT_COMPAT, so ignore
                break;
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
            case PERF_RECORD_COMM:
                stop = crtp_this->handle((const RecordCommType*)event_header_p);
                break;
            default:
                stop = crtp_this->handle((const RecordUnknownType*)event_header_p);
            }
            pop();
            if (stop)
            {
                break;
            }
        }
        Log::trace() << "read " << read_samples << " samples.";
    }

    void pop()
    {
        auto* ev = get();
        data_tail(data_tail() + ev->size);
    }

    bool empty()
    {
        return data_head() == data_tail();
    }

    perf_event_header* get()
    {
        auto cur_head = data_head();
        auto cur_tail = data_tail();

        assert(cur_tail <= cur_head);
        Log::trace() << "head: " << cur_head << ", tail: " << cur_tail;

        // Unless there is a serious kernel bug, the kernel will
        // always throw away
        // events on overflow and write PERF_RECORD_LOST events
        assert(cur_head - cur_tail <= data_size());

        auto d = data();

        auto index = cur_tail % data_size();
        auto event_header_p = (struct perf_event_header*)(d + index);
        auto len = event_header_p->size;

        assert(cur_tail + len <= cur_head);

        // Event spans the wrap-around of the ring buffer
        if (index + len > data_size())
        {
            std::byte* dst = event_copy;
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

        return event_header_p;
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
        return __atomic_load_n(&(header()->data_head), __ATOMIC_ACQUIRE);
    }

    void data_tail(uint64_t tail)
    {
        __atomic_store_n(&(header()->data_tail), tail, __ATOMIC_RELEASE);
    }

    // data_tail is only read in the kernel, so reads to data_tail in userspace do not have to be
    // protected.
    uint64_t data_tail() const
    {
        return header()->data_tail;
    }

    uint64_t data_size() const
    {
        // workaround for old kernels
        // assert(header()->data_size == mmap_pages_ * get_page_size());
        return mmap_pages_ * get_page_size();
    }

    std::byte* data()
    {
        // workaround for old kernels
        // assert(header()->data_offset == get_page_size());
        return reinterpret_cast<std::byte*>(base) + get_page_size();
    }

public:
    int fd()
    {
        return fd_;
    }

    bool handle(const RecordForkType*)
    {
        // It seems you get fork events even if not enabled via attr.task = true;
        // silently ignore it if our reader type is not specifically interested in forks
        return false;
    }

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
    int fd_;
    void* base;
    std::byte event_copy[PERF_SAMPLE_MAX_SIZE] __attribute__((aligned(8)));
};

class DummyCRTPWriter
{
};

using PullReader = EventReader<DummyCRTPWriter>;
} // namespace perf
} // namespace lo2s
