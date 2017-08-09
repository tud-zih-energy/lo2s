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

#include <cstddef>
#include <utility>

namespace lo2s
{

class SharedMmap
{
public:
    SharedMmap();
    SharedMmap(std::size_t size);
    SharedMmap(std::size_t size, int fd);

    SharedMmap(const SharedMmap&) = delete;
    SharedMmap& operator=(const SharedMmap&) = delete;

    SharedMmap(SharedMmap&& other) noexcept
    {
        addr_ = other.addr_;
        other.addr_ = nullptr;
    }

    SharedMmap& operator=(SharedMmap&& other) noexcept
    {
        // We are swapping here, so we can make this function noexcept
        std::swap(other.addr_, addr_);
        std::swap(other.lenght_, lenght_);
    }

    ~SharedMmap();

public:
    void map(std::size_t size);
    void map(std::size_t size, int fd);
    void unmap();

    void* get()
    {
        return addr_;
    }

private:
    std::size_t lenght_;
    void* addr_;
};
}
