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
#include <lo2s/rb/shm_ringbuf.hpp>
#include <lo2s/types/process.hpp>

#include <memory>

#include <cassert>
#include <cstddef>
#include <cstdint>

extern "C"
{
}

namespace lo2s
{
class RingbufWriter
{
public:
    RingbufWriter(Process process, RingbufMeasurementType type);

    const struct ringbuf_header* header()
    {
        return rb_->header();
    }

    void commit();

    template <class T>
    T* reserve(uint64_t payload = 0)
    {
        // No other reservation can be active!
        assert(reserved_size_ == 0);

        const uint64_t ev_size = sizeof(T) + payload;

        T* ev = reinterpret_cast<T*>(rb_->head(ev_size));
        if (ev == nullptr)
        {
            return nullptr;
        }

        memset(ev, 0, ev_size);
        ev->header.size = ev_size;

        reserved_size_ = ev_size;
        return ev;
    }

    uint64_t timestamp();

private:
    // Writes the fd of the shared memory to the Unix Domain Socket
    void write_fd(RingbufMeasurementType type);

    size_t reserved_size_ = 0;
    std::unique_ptr<ShmRingbuf> rb_;
};
} // namespace lo2s
