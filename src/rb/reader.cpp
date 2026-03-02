// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
