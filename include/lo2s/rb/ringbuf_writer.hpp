#pragma once

#include <lo2s/rb/header.hpp>
#include <lo2s/rb/shm_ringbuf.hpp>
#include <lo2s/shared_memory.hpp>
#include <lo2s/types/process.hpp>

#include <cstdint>

extern "C"
{
#include <sys/mman.h>
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

        uint64_t ev_size = sizeof(T) + payload;

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
