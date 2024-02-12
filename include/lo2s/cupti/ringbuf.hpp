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

#include <atomic>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>

extern "C"
{
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
};

#pragma once

struct ringbuf_header
{
    uint64_t version;
    uint64_t size;
    std::atomic_uint64_t head;
    std::atomic_uint64_t tail;
    std::atomic_uint64_t fill;
};

enum class CuptiEventType : uint64_t
{
    CUPTI_KERNEL = 0,
};

struct cupti_event_header
{
    CuptiEventType type;
    uint64_t size;
};

struct cupti_event_kernel
{
    struct cupti_event_header header;
    uint64_t start;
    uint64_t end;
    char name[1];
};

class RingBufDeleter
{
public:
    RingBufDeleter() : start_(nullptr), size_(0)
    {
    }

    RingBufDeleter(std::byte* start, size_t size) : start_(start), size_(size)
    {
    }

    void operator()(std::byte* p) const
    {
        if (p >= start_ && p < start_ + size_)
        {
            return;
        }
        else
        {
            free(p);
        }
    }

private:
    std::byte* start_;
    size_t size_;
};

typedef std::unique_ptr<std::byte, RingBufDeleter> rb_ptr;

class RingBufWriter
{
public:
    RingBufWriter(std::string filename, size_t size) : filename_(filename)
    {
        fd_ = shm_open(filename.c_str(), O_RDWR, 0600);
        if (fd_ == -1)
        {
            throw std::system_error(errno, std::system_category());
        }

        ringbuf_ = (struct ringbuf_header*)mmap(NULL, sizeof(struct ringbuf_header) + size,
                                                PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);

        if (ringbuf_ == MAP_FAILED)
        {
            throw std::system_error(errno, std::system_category());
        }

        start_ = (std::byte*)ringbuf_ + sizeof(struct ringbuf_header);
    }

    std::byte* reserve(size_t size)
    {
        if (ringbuf_->fill.load() + size > ringbuf_->size)
        {
            return nullptr;
        }

        if (reserved_ != nullptr)
        {
            // TODO: exception here
            return nullptr;
        }
        if (ringbuf_->head.load() + size > ringbuf_->size)
        {
            reserved_ = (std::byte*)malloc(size);
        }
        else
        {
            reserved_ = start_ + ringbuf_->head.load();
        }

        reserved_size_ = size;
        return reserved_;
    }

    bool commit()
    {
        if (reserved_ == nullptr)
        {
            // TODO: Turn into exception, maybe?
            return false;
        }

        if (!(reserved_ >= start_ && reserved_ < start_ + ringbuf_->size))
        {
            uint64_t wrap = ringbuf_->size - ringbuf_->head.load();
            memcpy(start_ + ringbuf_->head.load(), reserved_, wrap);
            memcpy(start_, reserved_ + wrap, reserved_size_ - wrap);

            free(reserved_);
        }
        ringbuf_->head.store((ringbuf_->head.load() + reserved_size_) % ringbuf_->size);
        ringbuf_->fill.fetch_add(reserved_size_);

        reserved_ = nullptr;
        reserved_size_ = 0;

        return true;
    }

private:
    std::string filename_;
    std::byte* start_;
    std::byte* reserved_ = nullptr;
    size_t reserved_size_ = 0;
    int fd_;
    struct ringbuf_header* ringbuf_;
};

class RingBufReader
{
public:
    RingBufReader(std::string filename, size_t size)
    {
        fd_ = shm_open(filename.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
        if (fd_ == -1)
        {
            throw std::system_error(errno, std::system_category());
        }

        ringbuf_ = (struct ringbuf_header*)mmap(NULL, sizeof(struct ringbuf_header) + size,
                                                PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);

        if (ringbuf_ == MAP_FAILED)
        {
            throw std::system_error(errno, std::system_category());
        }

        ftruncate(fd_, sizeof(struct ringbuf_header) + size);

        ringbuf_->version = 0;
        ringbuf_->size = size;
        ringbuf_->tail.store(0);
        ringbuf_->head.store(0);

        start_ = (std::byte*)ringbuf_ + sizeof(struct ringbuf_header);
        deleter_ = RingBufDeleter(start_, ringbuf_->size);
    }

    rb_ptr get(size_t size)
    {
        if (sizeof(size) > ringbuf_->fill.load())
        {
            return nullptr;
        }
        RingBufDeleter deleter_(start_, ringbuf_->size);

        if (ringbuf_->tail.load() + size <= ringbuf_->size)
        {
            return std::unique_ptr<std::byte, RingBufDeleter>(start_ + ringbuf_->tail.load(),
                                                              deleter_);
        }
        else
        {
            std::byte* res = (std::byte*)malloc(size);

            uint64_t wrap = ringbuf_->size - ringbuf_->tail.load();
            memcpy(res, start_ + ringbuf_->tail.load(), wrap);
            memcpy(res + wrap, start_, size - wrap);

            return rb_ptr(res, deleter_);
        }
    }

    bool pop(size_t size)
    {
        if (size > ringbuf_->fill.load())
        {
            return false;
        }

        ringbuf_->tail.store((ringbuf_->tail.load() + size) % ringbuf_->size);
        ringbuf_->fill.fetch_sub(size);

        return true;
    }

private:
    int fd_;
    std::byte* start_;
    struct ringbuf_header* ringbuf_;
    RingBufDeleter deleter_;
};
