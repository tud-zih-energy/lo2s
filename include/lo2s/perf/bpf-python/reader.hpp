/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <lo2s/perf/tracepoint/format.hpp>

#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/util.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

#include <filesystem>

#include <ios>

#include <cstddef>

extern "C"
{
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
}

#include <bcc/BPF.h>
#include <bpf/libbpf.h>

namespace lo2s
{
namespace perf
{
namespace bpf_python
{

const std::string BPF_PROGRAM = R"(
 #include <linux/sched.h>

 #include <uapi/linux/ptrace.h>
 struct event_t {
     bool type;
     int cpu;
     u64 time;
     char filename[32];
     char funcname[32];
 };
 BPF_RINGBUF_OUTPUT(events, 8);
 int on_function__entry(struct pt_regs *ctx) {
   struct event_t event = {};
   char *filename_ptr;
   char *funcname_ptr;
   event.type = false;
   bpf_usdt_readarg(1, ctx, &filename_ptr);
   bpf_usdt_readarg(2, ctx, &funcname_ptr);
   bpf_probe_read_user_str(event.filename, 32, filename_ptr);
   bpf_probe_read_user_str(event.funcname, 32, funcname_ptr);
   event.time = bpf_ktime_get_ns();
   event.cpu = ((struct task_struct *)bpf_get_current_task())->cpu;
   events.ringbuf_output(&event, sizeof(event), 0);
   return 0;
 }

 int on_function__return(struct pt_regs *ctx) {
   struct event_t event = {};
   char *filename_ptr;
   char *funcname_ptr;
   event.type = true;
   bpf_usdt_readarg(1, ctx, &filename_ptr);
   bpf_usdt_readarg(2, ctx, &funcname_ptr);
   bpf_probe_read_user_str(event.filename, 32, filename_ptr);
   bpf_probe_read_user_str(event.funcname, 32, funcname_ptr);
   event.time = bpf_ktime_get_ns();
   event.cpu = ((struct task_struct *)bpf_get_current_task())->cpu;
   events.ringbuf_output(&event, sizeof(event), 0);
   return 0;
 })";


 struct python_event
 {
     bool type;
     int cpu;
     uint64_t time;
     char filename[32];
     char funcname[16];
 };

class Reader
{
public:
    Reader()
    {
	ebpf::USDT u1(PY_BIN, "python", "function__entry", "on_function__entry");
     ebpf::USDT u2(PY_BIN, "python", "function__return", "on_function__return");
     ebpf::BPF* bpf = new ebpf::BPF();

     auto init_res = bpf->init(BPF_PROGRAM, {}, { u1, u2 });
     if (init_res.code() != 0)
     {
         std::cerr << init_res.msg() << std::endl;
         return 1;
     }

     auto attach_res = bpf->attach_usdt_all();
     if (attach_res.code() != 0)
     {
         std::cerr << attach_res.msg() << std::endl;
         return 1;
     }

     int err = 0;
     struct ring_buffer* rb = NULL;

     rb = ring_buffer__new(bpf->get_table("events").get_fd(), handle_output, NULL, NULL);
     if (!rb)
     {
         err = -1;
         fprintf(stderr, "Failed to create ring buffer\n");
         goto cleanup;
     }

     fd = ring_buffer__epoll_fd(rb);

    }
    
    ~Reader()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }

    int fd()
    {
        return fd;
    }

    void read()
    {
        ring_buffer__consume(rb);
    }

private:
    struct ring_buffer *rb;
    int fd_ = -1;
};
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
