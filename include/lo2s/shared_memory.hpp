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

#include <cassert>
#include <cstddef>

#include <sys/types.h>

extern "C"
{
#include <sys/mman.h>
}

namespace lo2s
{
class SharedMemory
{
public:
    SharedMemory() = default;

    SharedMemory(SharedMemory&) = delete;
    SharedMemory& operator=(SharedMemory&) = delete;

    SharedMemory(SharedMemory&& other) noexcept : addr_(other.addr_), size_(other.size_)
    {
        other.addr_ = nullptr;
    }

    SharedMemory& operator=(SharedMemory&& other) noexcept
    {
        unmap();
        addr_ = other.addr_;
        size_ = other.size_;

        other.addr_ = nullptr;

        return *this;
    }

    SharedMemory(int fd, size_t size, off_t offset = 0, void* location = nullptr) : size_(size)
    {
        if (location == nullptr)
        {
            addr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
        }
        else
        {
            addr_ =
                mmap(location, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, offset);
        }

        if (addr_ == MAP_FAILED)
        {
            throw_errno();
        }
    }

    template <typename T>
    T* as()
    {
        return reinterpret_cast<T*>(addr_);
    }

    template <typename T>
    const T* as() const
    {
        return reinterpret_cast<const T*>(addr_);
    }

    ~SharedMemory()
    {
        unmap();
    }

    size_t size() const
    {
        return size_;
    }

private:
    void unmap()
    {
        if (addr_ != nullptr)
        {
            munmap(addr_, size_);
        }
    }

    void* addr_ = nullptr;
    size_t size_ = 0;
};
} // namespace lo2s
