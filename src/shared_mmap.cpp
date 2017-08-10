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

#include <lo2s/error.hpp>
#include <lo2s/shared_mmap.hpp>

extern "C" {
#include <sys/mman.h>
}

namespace lo2s
{

SharedMmap::SharedMmap() : addr_(nullptr)
{
}

SharedMmap::SharedMmap(std::size_t size) : addr_(nullptr)
{
    map(size);
}

SharedMmap::SharedMmap(std::size_t size, int fd) : addr_(nullptr)
{
    map(size, fd);
}

SharedMmap::~SharedMmap()
{
    unmap();
}

void SharedMmap::map(std::size_t size)
{
    addr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // Should not be necessary to check for nullptr, but we've seen it!
    if (addr_ == MAP_FAILED || addr_ == nullptr)
    {
        throw_errno();
    }

    lenght_ = size;
}

void SharedMmap::map(std::size_t size, int fd)
{
    addr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    // Should not be necessary to check for nullptr, but we've seen it!
    if (addr_ == MAP_FAILED || addr_ == nullptr)
    {
        throw_errno();
    }

    lenght_ = size;
}

void SharedMmap::unmap()
{
    if (addr_ == nullptr || addr_ == MAP_FAILED)
    {
        return;
    }

    int ret = munmap(addr_, lenght_);

    addr_ = nullptr;

    if (ret != 0)
    {
        throw_errno();
    }
}
}
