// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    RingbufReader(clockid_t clockid);

    RingbufReader(const RingbufReader&) = delete;
    RingbufReader& operator=(const RingbufReader& other) = delete;

    RingbufReader(RingbufReader&& other) noexcept : rb_(std::move(other.rb_))
    {
    }

    RingbufReader& operator=(RingbufReader&& other) noexcept
    {
        this->rb_ = std::move(other.rb_);

        return *this;
    }

    ~RingbufReader() = default;

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

    int fd() const
    {
        return rb_->fd();
    }

    uint64_t get_top_event_type();
    // Check if we can atleast load an event header, if not, there are no new events
    bool empty();
    void pop();

private:
    std::unique_ptr<ShmRingbuf> rb_;
};
} // namespace lo2s
