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

#include <lo2s/rb/events.hpp>
#include <lo2s/rb/header.hpp>
#include <lo2s/rb/shm_ringbuf.hpp>

#include <memory>

#include <cstdint>

extern "C"
{
}

namespace lo2s
{
class RingbufReader
{
public:
    RingbufReader(int fd, clockid_t clockid);

    const struct ringbuf_header* header()
    {
        return rb_->header();
    }

    template <class T>
    const T* get()
    {
        const auto* header =
            reinterpret_cast<const struct event_header*>(rb_->tail(sizeof(struct event_header)));

        if (header == nullptr)
        {
            return nullptr;
        }

        const T* ev = reinterpret_cast<const T*>(rb_->tail(header->size));
        if (ev == nullptr)
        {
            return nullptr;
        }

        return ev;
    }

    uint64_t get_top_event_type();
    // Check if we can atleast load an event header, if not, there are no new events
    bool empty();
    void pop();

private:
    std::unique_ptr<ShmRingbuf> rb_;
};
} // namespace lo2s
