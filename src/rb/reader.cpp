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

#include <lo2s/rb/reader.hpp>

#include <lo2s/rb/events.hpp>
#include <lo2s/rb/shm_ringbuf.hpp>

#include <memory>
#include <stdexcept>

#include <cstdint>

namespace lo2s
{

RingbufReader::RingbufReader(int fd, clockid_t clockid) : rb_(std::make_unique<ShmRingbuf>(fd))
{
    rb_->header()->clockid = clockid;
    rb_->header()->lo2s_ready.store(1);
}

uint64_t RingbufReader::get_top_event_type()
{
    auto* header = reinterpret_cast<struct event_header*>(rb_->tail(sizeof(struct event_header)));

    if (header == nullptr)
    {
        throw std::runtime_error("get_top_event_type called without checking empty first!");
    }

    return header->type;
}

// Check if we can atleast load an event header, if not, there are no new events
bool RingbufReader::empty()
{
    auto* header = reinterpret_cast<struct event_header*>(rb_->tail(sizeof(struct event_header)));
    return header == nullptr;
}

void RingbufReader::pop()
{
    auto* header = reinterpret_cast<struct event_header*>(rb_->tail(sizeof(struct event_header)));
    if (header == nullptr)
    {
        return;
    }
    rb_->advance_tail(header->size);
}
} // namespace lo2s
