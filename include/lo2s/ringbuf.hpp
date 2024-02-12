/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
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
#include <lo2s/shared_memory.hpp>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <system_error>

extern "C"
{
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
}

namespace lo2s
{

// To resolve possible ringbuf format incompatibilities
#define RINGBUF_VERSION 1

struct ringbuf_header
{
    uint64_t version;
    uint64_t size;
    std::atomic_uint64_t head;
    std::atomic_uint64_t tail;
};

class ShmRingbuf
{
public:
    ShmRingbuf(std::string component, pid_t pid, bool create, size_t pages)
    {
        std::string filename = "/lo2s-" + component + "-" + std::to_string(pid);

        fd_ = shm_open(filename.c_str(), create ? O_RDWR | O_CREAT | O_EXCL : O_RDWR, 0600);
        if (fd_ == -1)
        {
            throw std::system_error(errno, std::system_category());
        }

        size_t pagesize = sysconf(_SC_PAGESIZE);
        size_t size;

        if (create)
        {
            size = pagesize * pages;
            ftruncate(fd_, size + sysconf(_SC_PAGESIZE));
        }
        else
        {
            auto header_map = SharedMemory(fd_, sizeof(struct ringbuf_header), 0);
            size = header_map.as<struct ringbuf_header>()->size;
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

        first_mapping_ = SharedMemory(fd_, size * 2 + pagesize, 0);

        second_mapping_ = SharedMemory(fd_, size, pagesize, first_mapping_.as<std::byte>() + size);

        header_ = first_mapping_.as<struct ringbuf_header>();
        start_ = first_mapping_.as<std::byte>() + pagesize;

        if (create)
        {
            header_->version = RINGBUF_VERSION;
            header_->size = size;
            header_->tail.store(0);
            header_->head.store(0);
        }
        else
        {
            if (header_->version != RINGBUF_VERSION)
            {
                throw new std::runtime_error("Incompatible RingBuffer Version " +
                                             std::to_string(header_->version) +
                                             " detected on other side!");
            }
        }
    }

    uint64_t head()
    {
        return header_->head.load();
    }

    uint64_t tail()
    {
        return header_->tail.load();
    }

    void head(uint64_t new_head)
    {
        return header_->head.store(new_head);
    }

    void tail(uint64_t new_tail)
    {
        return header_->tail.store(new_tail);
    }

    uint64_t size()
    {
        return header_->size;
    }

protected:
    std::byte* start_;

private:
    struct ringbuf_header* header_;
    int fd_;
    SharedMemory first_mapping_, second_mapping_;
};

class RingBufWriter : public ShmRingbuf
{
public:
    RingBufWriter(std::string component, pid_t pid, bool create, size_t pages = 0)
    : ShmRingbuf(component, pid, create, pages)
    {
    }

    std::byte* reserve(size_t ev_size)
    {
        if (ev_size == 0)
        {
            return nullptr;
        }

        // No other reservation can be active!
        assert(reserved_size_ == 0);

        if (head() >= tail() && ev_size >= tail() - head() + size())
        {
            return nullptr;
        }
        if (head() < tail() && ev_size >= tail() - head())
        {
            return nullptr;
        }

        reserved_size_ = ev_size;
        return start_ + head();
    }

    void commit()
    {
        assert(reserved_size_ != 0);

        head((head() + reserved_size_) % size());
        reserved_size_ = 0;
    }

private:
    size_t reserved_size_ = 0;
};

class RingBufReader : public ShmRingbuf
{
public:
    RingBufReader(std::string component, pid_t pid, bool create, size_t pages = 0)
    : ShmRingbuf(component, pid, create, pages)
    {
    }

    std::byte* get(size_t size)
    {
        if (size == 0)
        {
            return nullptr;
        }

        if (!can_be_loaded(size))
        {
            return nullptr;
        }
        return start_ + tail();
    }

    void pop(size_t ev_size)
    {
        if (ev_size == 0)
        {
            return;
        }

        // Calling pop() without trying to get() data from the ringbuffer first is an error
        assert(can_be_loaded(ev_size));

        tail((tail() + ev_size) % size());
    }

private:
    bool can_be_loaded(size_t ev_size)
    {
        if (tail() <= head())
        {
            return tail() + ev_size <= head();
        }

        return tail() + ev_size <= head() + size();
    }
};
} // namespace lo2s
