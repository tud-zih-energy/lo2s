#pragma once

#include <lo2s/rb/events.hpp>
#include <lo2s/rb/header.hpp>
#include <lo2s/rb/shm_ringbuf.hpp>

#include <memory>

extern "C"
{
#include <sys/types.h>
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
    T* get()
    {
        auto* header =
            reinterpret_cast<struct event_header*>(rb_->tail(sizeof(struct event_header)));

        if (header == nullptr)
        {
            return nullptr;
        }

        T* ev = reinterpret_cast<T*>(rb_->tail(header->size));
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
