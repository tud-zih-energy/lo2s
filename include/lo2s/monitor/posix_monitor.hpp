/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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

#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/fwd.hpp>

extern "C"
{
#include <bpf/libbpf.h>
#include <posix_io.skel.h>
#include <sys/types.h>
}

#include <lo2s/types/thread.hpp>

#include <memory>
#include <string>

#include <cstddef>

namespace lo2s::monitor
{
class PosixMonitor : public ThreadedMonitor
{
public:
    struct RingBufferDeleter
    {
        void operator()(struct ring_buffer* rb)
        {
            ring_buffer__free(rb);
        }
    };

    struct SkelDeleter
    {
        void operator()(struct posix_io_bpf* skel)
        {
            posix_io_bpf__destroy(skel);
        }
    };

    PosixMonitor(trace::Trace& trace);

    void insert_thread(Thread thread);
    void exit_thread(Thread thread);

    void initialize_thread() override
    {
    }

    // General assumption here: A thread will at all times only be in one read()/write() call.
    void handle_event(void* data, size_t datasz);

    void run() override;
    void stop() override;

    void finalize_thread() override
    {
    }

    void monitor()
    {
    }

    std::string group() const override
    {
        return "PosixMonitor";
    }

private:
    struct ThreadFd
    {
        ThreadFd() : fd(-1), thread(Thread::invalid())
        {
        }

        ThreadFd(int fd, pid_t thread) : fd(fd), thread(Thread(thread))
        {
        }

        friend bool operator<(const ThreadFd& lhs, const ThreadFd& rhs)
        {
            if (lhs.fd == rhs.fd)
            {
                return lhs.thread < rhs.thread;
            }

            return lhs.fd < rhs.fd;
        }

        friend bool operator==(const ThreadFd& lhs, const ThreadFd& rhs)
        {
            return lhs.fd == rhs.fd && lhs.thread == rhs.thread;
        }

    private:
        int fd;
        Thread thread;
    };

    trace::Trace& trace_;
    perf::time::Converter& time_converter_;

    std::map<Thread, int> last_fd_;
    std::map<Thread, uint64_t> last_count_;
    std::map<Thread, uint64_t> last_buf_;
    std::map<ThreadFd, int> instance_;

    std::unique_ptr<struct ring_buffer, RingBufferDeleter> rb_;
    std::unique_ptr<struct posix_io_bpf, SkelDeleter> skel_;

    bool stop_ = false;
};

} // namespace lo2s::monitor
