/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
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

#include <lo2s/error.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/helpers/mem_fd.hpp>
#include <lo2s/ringbuf_events.hpp>
#include <lo2s/shared_memory.hpp>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <system_error>

extern "C"
{
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
}

namespace lo2s
{

// To resolve possible ringbuf format incompatibilities
// Increase everytime you:
//  - change the ringbuf_header
//  - add, delete or change events
#define RINGBUF_VERSION 2

enum class RingbufMeasurementType : uint64_t
{
    GPU = 0,
    OPENMP = 1,
};

struct ringbuf_header
{
    uint64_t version;
    uint64_t size;
    std::atomic_uint64_t head;
    std::atomic_uint64_t tail;
    // set by the writer side (CUPTI, etc.)
    pid_t pid;

    // set by the reader size (lo2s)
    std::atomic<uint64_t> lo2s_ready;
    clockid_t clockid;
};

class ShmRingbuf
{
public:
    ShmRingbuf(WeakFd fd) : fd_(fd)
    {
        auto header_map = SharedMemory(fd_, sizeof(struct ringbuf_header), 0);

        size_t size = header_map.as<struct ringbuf_header>()->size;

        uint64_t version = header_map.as<struct ringbuf_header>()->version;
        if (version != RINGBUF_VERSION)
        {
            throw new std::runtime_error(std::string("Incompatible Ringbuffer Version") +
                                         std::to_string(RINGBUF_VERSION) +
                                         " (us) != " + std::to_string(version) + " (other side)!");
        }

        // To handle events that wrap around the ringbuffer, map it twice into virtual memory
        // back-to-back. This way events that wrap around the ringbuffer can be read and written
        // without noticing the wraparound:
        //
        // in physical memory: [ent|-----|ev]
        //
        // in virtual memory:  [entcg|-----|ev][ent----|ev]
        //
        // As there is no way to reserve a range of virtual memory, mmap()-ing two adjacent
        // ring-buffer without races is tricky. We solve this problem by mmap()-ing an area twice
        // the size of the ringbuffer and then overwriting the latter half of this mapping with
        // another mapping of the ringbuffer using MMAP_FIXED. This way we only touch mappings we
        // control. Also, put the ringbuffer header on a separate page to make life easier.

        size_t pagesize = sysconf(_SC_PAGESIZE);

        first_mapping_ = SharedMemory(fd_, size * 2 + pagesize, 0);

        second_mapping_ =
            SharedMemory(fd_, size, pagesize, first_mapping_.as<std::byte>() + size + pagesize);

        header_ = first_mapping_.as<struct ringbuf_header>();
        start_ = first_mapping_.as<std::byte>() + pagesize;
    }

    std::byte* head(size_t ev_size)
    {

        uint64_t head = header_->head.load();
        uint64_t tail = header_->tail.load();

        if (head >= tail)
        {
            if (head + ev_size > tail + header_->size)
            {
                return nullptr;
            }
        }

        else
        {
            if (head + ev_size >= tail)
            {
                return nullptr;
            }
        }
        return start_ + head;
    }

    std::byte* tail(uint64_t ev_size)
    {
        if (!can_be_loaded(ev_size))
        {
            return nullptr;
        }
        return start_ + header_->tail.load();
    }

    void advance_head(size_t ev_size)
    {
        header_->head.store((header_->head.load() + ev_size) % header_->size);
    }

    void advance_tail(size_t ev_size)
    {
        // Calling pop() without trying to get() data from the ringbuffer first is an error
        assert(can_be_loaded(ev_size));

        header_->tail.store((header_->tail.load() + ev_size) % header_->size);
    }

    bool can_be_loaded(size_t ev_size)
    {
        uint64_t head = header_->head.load();
        uint64_t tail = header_->tail.load();
        if (tail <= head)
        {
            return tail + ev_size <= head;
        }
        else
        {
            return tail + ev_size <= head + header_->size;
        }
    }

    WeakFd get_weak_fd()
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
    WeakFd fd_;
    SharedMemory first_mapping_, second_mapping_;
};

class RingbufWriter
{
public:
    RingbufWriter(Process process, RingbufMeasurementType type)
    {
        memory_ = std::make_unique<MemFd>(MemFd::create("lo2s", true).unpack_ok());

        size_t pagesize = sysconf(_SC_PAGESIZE);
        size_t size;

        size_t pages = 16;

        char* lo2s_rb_size = getenv("LO2S_RB_SIZE");
        if (lo2s_rb_size != nullptr)
        {
            pages = std::atoi(lo2s_rb_size);
            if (pages < 1)
            {
                throw std::runtime_error("Invalid amount of ringbuffer pages: " +
                                         std::to_string(pages));
            }
        }

        size = pagesize * pages;

        memory_->set_size(size + sysconf(_SC_PAGESIZE)).throw_if_error();

        // Prevent the other process from modifying the size of the ringbuffer.
        memory_->seal_grow().throw_if_error();
        memory_->seal_shrink().throw_if_error();
        
        // Prevent the other process from modifying the previously set seals.
        memory_->seal_sealing().throw_if_error();

        auto header_map = SharedMemory(memory_->to_weak(), sizeof(struct ringbuf_header), 0);

        struct ringbuf_header* first_header = header_map.as<struct ringbuf_header>();

        memset((void*)first_header, 0, sizeof(struct ringbuf_header));

        first_header->version = RINGBUF_VERSION;
        first_header->size = size;
        first_header->tail.store(0);
        first_header->head.store(0);
        first_header->lo2s_ready = false;
        first_header->pid = process.as_pid_t();

        rb_ = std::make_unique<ShmRingbuf>(memory_->to_weak());
        write_fd(type);

        // Wait for other side to open connection
        while (!rb_->header()->lo2s_ready.load())
        {
        };
    }

    const struct ringbuf_header* header()
    {
        return rb_->header();
    }

    void commit()
    {
        assert(reserved_size_ != 0);
        rb_->advance_head(reserved_size_);
        reserved_size_ = 0;
    }

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

    uint64_t timestamp()
    {
        assert(rb_->header()->lo2s_ready.load());
        struct timespec ts;
        clock_gettime(rb_->header()->clockid, &ts);
        uint64_t res = ts.tv_sec * 1000000000 + ts.tv_nsec;
        return res;
    }

    bool finalized()
    {
        return finalized_;
    }

    // OpenMP loves to call its measurement callbacks beyond the lifetime of the
    // main thread (after it has returned from main()), which makes all the uses of RingbufWriter in
    // the other OpenMP threads UB.
    //
    // To alleviate this, set finalized to true in the destructor of RingbufWriter. Hopefully,
    // enough of the object remains to atleast check that member.
    ~RingbufWriter()
    {
        close(data_socket_);
        finalized_ = true;
    }

private:
    // Writes the fd of the shared memory to the Unix Domain Socket
    void write_fd(RingbufMeasurementType type)
    {
        char* socket_path = getenv("LO2S_SOCKET");
        if (socket_path == nullptr)
        {
            throw std::runtime_error("LO2S_SOCKET is not set!");
        }

        data_socket_ = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if (data_socket_ == -1)
        {
            throw std::system_error(errno, std::system_category());
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));

        addr.sun_family = AF_UNIX;

        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

        int ret = connect(data_socket_, (const struct sockaddr*)&addr, sizeof(addr));
        if (ret < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        union
        {
            struct cmsghdr cm;
            char control[CMSG_SPACE(sizeof(int))];
        } control_un;

        struct msghdr msg;
        msg.msg_control = control_un.control;
        msg.msg_controllen = sizeof(control_un.control);

        struct cmsghdr* cmptr;
        cmptr = CMSG_FIRSTHDR(&msg);
        cmptr->cmsg_len = CMSG_LEN(sizeof(int));
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;

        *((int*)CMSG_DATA(cmptr)) = rb_->get_weak_fd().as_int();

        msg.msg_name = NULL;
        msg.msg_namelen = 0;

        // We need to send some data with the fd anyways, so use that to send
        // the type of the measurement that we are doing.
        struct iovec iov[1];
        iov[0].iov_base = &type;
        iov[0].iov_len = 8;

        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        sendmsg(data_socket_, &msg, 0);
    }

    size_t reserved_size_ = 0;
    std::unique_ptr<ShmRingbuf> rb_;
    bool finalized_ = false;
    int data_socket_;
    std::unique_ptr<MemFd> memory_;
};

class RingbufReader
{
public:
    RingbufReader(WeakFd fd, clockid_t clockid) : rb_(std::make_unique<ShmRingbuf>(fd))
    {
        rb_->header()->clockid = clockid;
        rb_->header()->lo2s_ready.store(1);
    }

    const struct ringbuf_header* header()
    {
        return rb_->header();
    }

    template <class T>
    T* get()
    {
        struct event_header* header =
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

    uint64_t get_top_event_type()
    {
        struct event_header* header =
            reinterpret_cast<struct event_header*>(rb_->tail(sizeof(struct event_header)));

        if (header == nullptr)
        {
            throw std::runtime_error("get_top_event_type called without checking empty first!");
        }

        return header->type;
    }

    // Check if we can atleast load an event header, if not, there are no new events
    bool empty()
    {
        struct event_header* header =
            reinterpret_cast<struct event_header*>(rb_->tail(sizeof(struct event_header)));
        return header == nullptr;
    }

    void pop()
    {
        struct event_header* header =
            reinterpret_cast<struct event_header*>(rb_->tail(sizeof(struct event_header)));
        if (header == nullptr)
        {
            return;
        }
        rb_->advance_tail(header->size);
    }

private:
    std::unique_ptr<ShmRingbuf> rb_;
};
} // namespace lo2s
