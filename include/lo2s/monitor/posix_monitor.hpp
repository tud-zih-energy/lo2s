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

#include "otf2xx/common.hpp"
#include "otf2xx/event/io_create_handle.hpp"
#include "otf2xx/event/io_delete_file.hpp"
#include "otf2xx/event/io_destroy_handle.hpp"
#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/perf/time/converter.hpp>

extern "C"
{
#include <bpf/libbpf.h>
#include <lo2s/open.skel.h>
#include <lo2s/perf/posix_io/common.h>
#include <sys/resource.h>
}
namespace lo2s
{
namespace monitor
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
        void operator()(struct open_bpf* skel)
        {
            open_bpf__destroy(skel);
        }
    };

    PosixMonitor(trace::Trace& trace)
    : ThreadedMonitor(trace, "open() monitor"), trace_(trace),
      time_converter_(perf::time::Converter::instance())
    {
        // Need to bump memlock rlimit to run anything but the most trivial BPF programs

        struct rlimit rlim_new;
        rlim_new.rlim_cur = RLIM_INFINITY;
        rlim_new.rlim_max = RLIM_INFINITY;

        if (setrlimit(RLIMIT_MEMLOCK, &rlim_new))
        {
            throw_errno();
        }

        skel_ = std::unique_ptr<struct open_bpf, SkelDeleter>(open_bpf__open_and_load());
        if (!skel_)
        {
            return;
        }

        open_bpf__attach(skel_.get());

        rb_ = std::unique_ptr<struct ring_buffer, RingBufferDeleter>(
            ring_buffer__new(bpf_map__fd(skel_.get()->maps.rb), event_cb, this, NULL));

        if (!rb_)
        {
            return;
        }
    }

    static int libbpf_print_fn(enum libbpf_print_level level, const char* format, va_list args)
    {
        if (level > LIBBPF_INFO)
            return 0;
        return vfprintf(stderr, format, args);
    }

    void insert_thread(Thread thread [[maybe_unused]])
    {
        char insert = 1;
        pid_t pid = thread.as_pid_t();
        bpf_map__update_elem(skel_->maps.pids, &pid, sizeof(pid), &insert, sizeof(char), BPF_ANY);

        last_fd_[thread] = -1;
    }
    void exit_thread(Thread thread [[maybe_unused]])
    {
        pid_t pid = thread.as_pid_t();
        bpf_map__delete_elem(skel_->maps.pids, &pid, sizeof(pid), BPF_ANY);
    }

    void initialize_thread() override
    {
    }

    void handle_event(void* data, size_t datasz [[maybe_unused]])
    {
        struct posix_event_header* e = (struct posix_event_header*)data;
        ThreadFd thread_fd = ThreadFd(e->fd, e->pid);
        Thread thread = Thread(e->pid);
        std::string filename;
        if (e->fd == 0)
        {
            filename = "stdin";
        }
        else if (e->fd == 1)
        {
            filename = "stdout";
        }
        else if (e->fd == 2)
        {
            filename = "stderr";
        }
        if (e->type == OPEN)
        {
            struct open_event* e = (struct open_event*)data;

            if (!instance_.count(thread_fd))
            {
                instance_.emplace(thread_fd, 0);
            }
            if (e->header.fd >= 3)
            {
                filename = std::string(e->filename);
            }
            auto& handle = trace_.posix_io_handle(Thread(e->header.pid), e->header.fd,
                                                  instance_[thread_fd], filename);
            otf2::writer::local& writer = trace_.posix_io_writer(thread);

            writer << otf2::event::io_create_handle(time_converter_(e->header.time), handle,
                                                    otf2::common::io_access_mode_type::read_write,
                                                    otf2::common::io_creation_flag_type::none,
                                                    otf2::common::io_status_flag_type::none);
        }
        if (e->type == CLOSE)
        {
            auto& handle =
                trace_.posix_io_handle(Thread(e->pid), e->fd, instance_[thread_fd], filename);
            if (instance_.count(thread_fd))
            {
                instance_[thread_fd] = instance_[thread_fd] + 1;
            }

            otf2::writer::local& writer = trace_.posix_io_writer(thread);

            writer << otf2::event::io_destroy_handle(time_converter_(e->time), handle);
        }

        otf2::common::io_operation_mode_type mode = otf2::common::io_operation_mode_type::flush;
        if (e->type == READ_ENTER || e->type == READ_EXIT)
        {
            mode = otf2::common::io_operation_mode_type::read;
        }
        else
        {
            mode = otf2::common::io_operation_mode_type::write;
        }

        if (e->type == READ_ENTER || e->type == WRITE_ENTER)
        {
            struct read_write_event* event = (read_write_event*)data;
            last_fd_[thread] = event->header.fd;
            last_buf_[thread] = event->buf;
            last_count_[thread] = event->count;

            otf2::writer::local& writer = trace_.posix_io_writer(thread);
            otf2::definition::io_handle& handle =
                trace_.posix_io_handle(thread, event->header.fd, instance_[thread_fd], filename);
            writer << otf2::event::io_operation_begin(
                time_converter_(event->header.time), handle, mode,
                otf2::common::io_operation_flag_type::non_blocking, event->count, event->buf);
        }
        else if (e->type == READ_EXIT || e->type == WRITE_EXIT)
        {
            Thread thread(e->pid);

            if (last_fd_[thread] == -1)
            {
                return;
            }
            otf2::writer::local& writer = trace_.posix_io_writer(thread);
            otf2::definition::io_handle& handle = trace_.posix_io_handle(
                thread, last_fd_[thread], instance_[ThreadFd(last_fd_[thread], thread.as_pid_t())],
                filename);

            writer << otf2::event::io_operation_complete(time_converter_(e->time), handle,
                                                         last_count_[thread], last_buf_[thread]);
            last_fd_[thread] = -1;
        }
    }

    void run() override
    {
        while (!stop_)
        {
            ring_buffer__poll(rb_.get(), 100);
        }
    }

    void finalize_thread() override
    {
    }

    void stop() override
    {
        stop_ = true;
        thread_.join();
    }
    static int event_cb(void* ctx, void* data, size_t data_sz)
    {
        ((PosixMonitor*)ctx)->handle_event(data, data_sz);
        return 0;
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
    std::unique_ptr<struct open_bpf, SkelDeleter> skel_;
    bool stop_ = false;
};

} // namespace monitor
} // namespace lo2s
