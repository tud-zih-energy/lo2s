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
#include <lo2s/monitor/posix_monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/threaded_monitor.hpp>
#include <lo2s/perf/posix_io/common.h>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types/thread.hpp>

#include <otf2xx/common.hpp>
#include <otf2xx/definition/io_handle.hpp>
#include <otf2xx/event/io_create_handle.hpp>
#include <otf2xx/event/io_destroy_handle.hpp>
#include <otf2xx/writer/local.hpp>

#include <string>

#include <cstddef>

#include <linux/bpf.h>
#include <sched.h>
#include <sys/resource.h>

namespace lo2s::monitor
{
namespace
{

int event_cb(void* ctx, void* data, size_t data_sz)
{
    reinterpret_cast<PosixMonitor*>(ctx)->handle_event(data, data_sz);
    return 0;
}
} // namespace

PosixMonitor::PosixMonitor(trace::Trace& trace)
: ThreadedMonitor(trace, "open() monitor"), trace_(trace),
  time_converter_(perf::time::Converter::instance())
{
    // Need to bump memlock rlimit to run anything but the most trivial BPF programs
    struct rlimit rlim_new;
    rlim_new.rlim_cur = RLIM_INFINITY;
    rlim_new.rlim_max = RLIM_INFINITY;

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new))
    {
        Log::error() << "Could not increase memlock rlimit, can not load POSIX I/O BPF program";
        throw_errno();
    }

    skel_ = std::unique_ptr<struct posix_io_bpf, SkelDeleter>(posix_io_bpf__open_and_load());
    if (!skel_)
    {
        Log::error() << "Could not open and load POSIX I/O BPF program";
        throw_errno();
    }

    if (posix_io_bpf__attach(skel_.get()) < 0)
    {
        Log::error() << "Could not attach POSIX I/O BPF program to probe points";
        throw_errno();
    }

    rb_ = std::unique_ptr<struct ring_buffer, RingBufferDeleter>(
        ring_buffer__new(bpf_map__fd(skel_.get()->maps.rb), event_cb, this, NULL));

    if (!rb_)
    {
        Log::error() << "Could not attach to POSIX I/O BPF ring buffer";
        throw_errno();
    }
}

// Inserts new thread into list of threads whose POSIX I/O should be recorded.
// This information is communicated to the BPF program via a BPF map.
void PosixMonitor::insert_thread(Thread thread)
{
    char insert = 1;
    pid_t pid = thread.as_int();
    bpf_map__update_elem(skel_->maps.pids, &pid, sizeof(pid), &insert, sizeof(char), BPF_ANY);

    last_fd_[thread] = -1;
}

// Removes thread from list of threads whose POSIX I/O should be recorded
void PosixMonitor::exit_thread(Thread thread)
{
    pid_t pid = thread.as_int();
    bpf_map__delete_elem(skel_->maps.pids, &pid, sizeof(pid), BPF_ANY);
}

// General assumption here: A thread will at all times only be in one read()/write() call.
void PosixMonitor::handle_event(void* data, size_t datasz [[maybe_unused]])
{
    auto* e = reinterpret_cast<struct posix_event_header*>(data);

    ThreadFd const thread_fd(e->fd, e->pid);
    Thread const thread(e->pid);

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
        auto* e = reinterpret_cast<struct open_event*>(data);

        if (e->header.fd >= 3)
        {
            filename = std::string(e->filename);
        }

        // The same fd  can be used for multiple files in the same thread.
        // To get an unique mapping  from {tid, fd} to file, track the number of open()s that
        // returned the same fd. Everytime an fd is reused, its reuse count is increased. {tid,
        // fd, reuse count} => file should be unique.
        // NOLINTBEGIN (bugprone-branch-clone)
        if (!instance_.count(thread_fd))
        {
            instance_[thread_fd] = 0;
        }
        else
        {
            instance_[thread_fd]++;
        }
        // NOLINTEND (bugprone-branch-clone)

        auto& handle = trace_.posix_io_handle(Thread(e->header.pid), e->header.fd,
                                              instance_[thread_fd], filename);

        // NOLINTBEGIN(misc-const-correctness
        otf2::writer::local& writer = trace_.posix_io_writer(thread);

        writer << otf2::event::io_create_handle(
            time_converter_(e->header.time), handle, otf2::common::io_access_mode_type::read_write,
            otf2::common::io_creation_flag_type::none, otf2::common::io_status_flag_type::none);
        // NOLINTEND (misc-const-correctness)
    }
    if (e->type == CLOSE)
    {
        auto& handle =
            trace_.posix_io_handle(Thread(e->pid), e->fd, instance_[thread_fd], filename);

        // NOLINTBEGIN (misc-const-correctness)
        otf2::writer::local& writer = trace_.posix_io_writer(thread);

        writer << otf2::event::io_destroy_handle(time_converter_(e->time), handle);
        // NOLINTEND (misc-const-correctness)
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
        auto* event = reinterpret_cast<read_write_event*>(data);

        // When writing the IoOperationComplete event for a Begin, we have to use the fd of the
        // I/O operation as well as the number of bytes written and the pointer to the
        // read/write operation's source/destination buffer. The sys_exit_(read/write)
        // tracepoints do not supply this information, so cache it here for later use.
        last_fd_[thread] = event->header.fd;
        last_buf_[thread] = event->buf;
        last_count_[thread] = event->count;

        // NOLINTBEGIN(misc-const-correctness)
        otf2::writer::local& writer = trace_.posix_io_writer(thread);
        otf2::definition::io_handle& handle =
            trace_.posix_io_handle(thread, event->header.fd, instance_[thread_fd], filename);

        // OTF-2 requires IoOperation's to contain a matching_id so that it can match a
        // IoOperationBegin to the correct IoOperationComplete event. The matching_id of all
        // IoOperation's currently in flight has to be unique. As a matching_id, we use the
        // address of the source/destination memory buffer of the read/write. A thread should
        // be only in one read/write call at any given time, so this matching_id should fulfill
        // the uniqueness criterium.
        writer << otf2::event::io_operation_begin(
            time_converter_(event->header.time), handle, mode,
            otf2::common::io_operation_flag_type::non_blocking, event->count, event->buf);
        // NOLINTEND(misc-const-correctness)
    }
    else if (e->type == READ_EXIT || e->type == WRITE_EXIT)
    {
        Thread const thread(e->pid);

        if (last_fd_[thread] == -1)
        {
            return;
        }

        // NOLINTBEGIN (misc-const-correctness)
        otf2::writer::local& writer = trace_.posix_io_writer(thread);

        otf2::definition::io_handle& handle = trace_.posix_io_handle(
            thread, last_fd_[thread], instance_[ThreadFd(last_fd_[thread], thread.as_int())],
            filename);

        writer << otf2::event::io_operation_complete(time_converter_(e->time), handle,
                                                     last_count_[thread], last_buf_[thread]);
        // NOLINTEND (misc-const-correctness)
        last_fd_[thread] = -1;
    }
}

void PosixMonitor::run()
{
    while (!stop_)
    {
        ring_buffer__poll(rb_.get(), 100);
    }
}

void PosixMonitor::stop()
{
    stop_ = true;
    thread_.join();
}

} // namespace lo2s::monitor
