// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/rb/reader.hpp>

#include <lo2s/rb/events.hpp>
#include <lo2s/rb/header.hpp>
#include <lo2s/rb/shm_ringbuf.hpp>

#include <memory>
#include <stdexcept>
#include <system_error>

#include <cerrno>
#include <cstdint>
#include <cstring>

extern "C"
{
#include <sys/mman.h>
#include <unistd.h>
}

namespace lo2s
{

RingbufReader::RingbufReader(clockid_t clockid)
{
    const int fd = memfd_create("lo2s", 0);
    if (fd == -1)
    {
        throw ::std::system_error(errno, std::system_category());
    }

    size_t const pagesize = sysconf(_SC_PAGESIZE);
    int size = 0;

    const int pages = 16;

    size = static_cast<int>(pagesize) * pages;

    if (ftruncate(fd, size + sysconf(_SC_PAGESIZE)) == -1)
    {
        close(fd);
        throw std::system_error(errno, std::system_category());
    }

    auto header_map = SharedMemory(fd, sizeof(struct ringbuf_header), 0);

    auto* first_header = header_map.as<struct ringbuf_header>();

    memset(reinterpret_cast<void*>(first_header), 0, sizeof(struct ringbuf_header));

    first_header->version = RINGBUF_VERSION;
    first_header->size = size;
    first_header->tail.store(0);
    first_header->head.store(0);

    rb_ = std::make_unique<ShmRingbuf>(fd);
    rb_->header()->clockid = clockid;
}

uint64_t RingbufReader::get_top_event_type()
{
    const auto* header =
        reinterpret_cast<const struct event_header*>(rb_->tail(sizeof(struct event_header)));

    if (header == nullptr)
    {
        throw std::runtime_error("get_top_event_type called without checking empty first!");
    }

    return header->type;
}

// Check if we can atleast load an event header, if not, there are no new events
bool RingbufReader::empty()
{
    const auto* header =
        reinterpret_cast<const struct event_header*>(rb_->tail(sizeof(struct event_header)));
    return header == nullptr;
}

void RingbufReader::pop()
{
    const auto* header =
        reinterpret_cast<const struct event_header*>(rb_->tail(sizeof(struct event_header)));
    if (header == nullptr)
    {
        return;
    }
    rb_->advance_tail(header->size);
}
} // namespace lo2s
