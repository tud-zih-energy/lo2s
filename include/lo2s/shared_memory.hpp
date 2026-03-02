// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
