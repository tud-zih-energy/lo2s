// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/rb/header.hpp>
#include <lo2s/shared_memory.hpp>

#include <cstddef>
#include <cstdint>

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
