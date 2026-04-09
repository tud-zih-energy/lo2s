// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/rb/shm_ringbuf.hpp>

#include <lo2s/rb/header.hpp>
#include <lo2s/util.hpp>

#include <stdexcept>
#include <string>

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace lo2s
{
ShmRingbuf::ShmRingbuf(int fd) : fd_(fd)
{
    auto header_map = SharedMemory(fd_, sizeof(struct ringbuf_header), 0);

    size_t const size = header_map.as<struct ringbuf_header>()->size;

    uint64_t const version = header_map.as<struct ringbuf_header>()->version;
    if (version != RINGBUF_VERSION)
    {
        throw std::runtime_error("Incompatible Ringbuffer Version" +
                                 std::to_string(RINGBUF_VERSION) +
                                 " (us) != " + std::to_string(version) + " (other side)!");
    }

    // To handle events that wrap around the ringbuffer, map it twice into virtual memory
    // back-to-back. This way events that wrap around the ringbuffer can be read and written
    // without noticing the wraparound:
    //
    // in physical memory: [ent|-----|ev]
    //
    // in virtual memory:  [ent|-----|ev][ent----|ev]
    //
    // As there is no way to reserve a range of virtual memory, mmap()-ing two adjacent
    // ring-buffer without races is tricky. We solve this problem by mmap()-ing an area twice
    // the size of the ringbuffer and then overwriting the latter half of this mapping with
    // another mapping of the ringbuffer using MMAP_FIXED. This way we only touch mappings we
    // control. Also, put the ringbuffer header on a separate page to make life easier.

    size_t const pagesize = sysconf(_SC_PAGESIZE);

    first_mapping_ = SharedMemory(fd_, (size * 2) + pagesize, 0);

    second_mapping_ =
        SharedMemory(fd_, size, pagesize, first_mapping_.as<std::byte>() + size + pagesize);

    header_ = first_mapping_.as<struct ringbuf_header>();
    start_ = first_mapping_.as<std::byte>() + pagesize;
}

std::byte* ShmRingbuf::head(size_t ev_size)
{
    // Always round to the nearest multiple of 8, cause of alignment.
    ev_size = (ev_size + 7) & ~7;
    uint64_t const head = header_->head.load();
    uint64_t const tail = header_->tail.load();
    assert(tail % 8 == 0);
    assert(head % 8 == 0);

    if (head >= tail)
    {
        if (head + ev_size > tail + header_->size)
        {
            return nullptr;
        }
    }

    else
    {
        if (head + ev_size >= tail)
        {
            return nullptr;
        }
    }
    std::byte* res = start_ + head;

    if (reinterpret_cast<uint64_t>(res) % 8 != 0)
    {
        throw std::runtime_error("Got non-aligned tail!");
    }
    return res;
}

std::byte* ShmRingbuf::tail(uint64_t ev_size)
{
    ev_size = (ev_size + 7) & ~7;
    if (!can_be_loaded(ev_size))
    {
        return nullptr;
    }
    std::byte* res = start_ + header_->tail.load();

    if (reinterpret_cast<uint64_t>(res) % 8 != 0)
    {
        throw std::runtime_error("Got non-aligned tail!");
    }
    return res;
}

void ShmRingbuf::advance_head(size_t ev_size)
{
    ev_size = (ev_size + 7) & ~7;
    header_->head.store((header_->head.load() + ev_size) % header_->size);
}

void ShmRingbuf::advance_tail(size_t ev_size)
{
    ev_size = (ev_size + 7) & ~7;
    // Calling pop() without trying to get() data from the ringbuffer first is an error
    assert(can_be_loaded(ev_size));

    assert((header_->tail.load() + ev_size) % 8 == 0);
    header_->tail.store((header_->tail.load() + ev_size) % header_->size);
}

bool ShmRingbuf::can_be_loaded(size_t ev_size)
{
    ev_size = (ev_size + 7) & ~7;
    uint64_t const head = header_->head.load();
    uint64_t const tail = header_->tail.load();

    assert(tail % 8 == 0);
    assert(head % 8 == 0);

    if (tail <= head)
    {
        return tail + ev_size <= head;
    }
    return tail + ev_size <= head + header_->size;
}
} // namespace lo2s
