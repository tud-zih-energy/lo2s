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

#pragma once

#include <lo2s/rb/header.hpp>
#include <lo2s/shared_memory.hpp>

namespace lo2s
{
class ShmRingbuf
{
public:
    ShmRingbuf(int fd);

    std::byte* head(size_t ev_size);
    std::byte* tail(uint64_t ev_size);

    void advance_head(size_t ev_size);
    void advance_tail(size_t ev_size);

    bool can_be_loaded(size_t ev_size);

    int fd() const
    {
        return fd_;
    }

    struct ringbuf_header* header()
    {
        return header_;
    }

    std::byte* start_;

private:
    struct ringbuf_header* header_;
    int fd_;
    SharedMemory first_mapping_, second_mapping_;
};
} // namespace lo2s
