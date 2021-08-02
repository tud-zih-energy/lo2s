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

#include <lo2s/perf/bpf-python/writer.hpp>

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


class Reader
{
public:
    Reader(trace::Trace &trace) : 
            stop_(false), writer_(trace)
    {
	 ebpf::USDT u1(config().python_binary, "python", "function__entry", "on_function__entry");
     ebpf::USDT u2(config().python_binary, "python", "function__return", "on_function__return");
     ebpf::BPF* bpf = new ebpf::BPF();

     auto init_res = bpf->init(BPF_PROGRAM, {}, { u1, u2 });
     if (init_res.code() != 0)
     {
         std::cerr << init_res.msg() << std::endl;
     }

     auto attach_res = bpf->attach_usdt_all();
     if (attach_res.code() != 0)
     {
         std::cerr << attach_res.msg() << std::endl;
     }

     rb = ring_buffer__new(bpf->get_table("events").get_fd(), Reader::handle_output, &writer_, NULL);
     if (!rb)
     {
         fprintf(stderr, "Failed to create ring buffer\n");
     }

    }
    
    static int handle_output(void *ctx, void *data, size_t data_size)
    {
        static_cast<Writer*>(ctx)->write(data);

        return 0;
    }
    
    void start()
    {
        std::cerr << "MEEPOE\n";
        while(!stop_)
        {
            ring_buffer__poll(rb, 100);
        }
    }

    void stop()
    {
        stop_ = true;
    }

private:
    bool stop_;
    Writer writer_;
    struct ring_buffer *rb;
};
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
