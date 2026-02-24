/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2026,
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

    size_t const pagesize = get_page_size();

    first_mapping_ = SharedMemory(fd_, (size * 2) + pagesize, 0);

    second_mapping_ =
        SharedMemory(fd_, size, pagesize, first_mapping_.as<std::byte>() + size + pagesize);

    header_ = first_mapping_.as<struct ringbuf_header>();
    start_ = first_mapping_.as<std::byte>() + pagesize;
}

std::byte* ShmRingbuf::head(size_t ev_size)
{

    uint64_t const head = header_->head.load();
    uint64_t const tail = header_->tail.load();

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
    return start_ + head;
}

std::byte* ShmRingbuf::tail(uint64_t ev_size)
{
    if (!can_be_loaded(ev_size))
    {
        return nullptr;
    }
    return start_ + header_->tail.load();
}

void ShmRingbuf::advance_head(size_t ev_size)
{
    header_->head.store((header_->head.load() + ev_size) % header_->size);
}

void ShmRingbuf::advance_tail(size_t ev_size)
{
    // Calling pop() without trying to get() data from the ringbuffer first is an error
    assert(can_be_loaded(ev_size));

    header_->tail.store((header_->tail.load() + ev_size) % header_->size);
}

bool ShmRingbuf::can_be_loaded(size_t ev_size)
{
    uint64_t const head = header_->head.load();
    uint64_t const tail = header_->tail.load();
    if (tail <= head)
    {
        return tail + ev_size <= head;
    }
    return tail + ev_size <= head + header_->size;
}
} // namespace lo2s
