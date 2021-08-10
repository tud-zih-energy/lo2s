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

//Whenever you make changes to the BPF program uncomment the "-quotes and run clang-format on it
// Please tell me if you find a better way to do this

R"(

#include <linux/sched.h>

#include <uapi/linux/ptrace.h>
struct event_t
{
    bool type;
    int cpu;
    u32 tid;
    u64 time;
    char filename[32];
    char funcname[32];
};

BPF_RINGBUF_OUTPUT(events, 8);
int on_function__entry(struct pt_regs* ctx)
{
    struct event_t event = {};
    char* filename_ptr;
    char* funcname_ptr;
    event.type = false;
    bpf_usdt_readarg(1, ctx, &filename_ptr);
    bpf_usdt_readarg(2, ctx, &funcname_ptr);
    bpf_probe_read_user_str(event.filename, 32, filename_ptr);
    bpf_probe_read_user_str(event.funcname, 32, funcname_ptr);
    event.time = bpf_ktime_get_ns();
    event.cpu = ((struct task_struct*)bpf_get_current_task())->cpu;
    events.ringbuf_output(&event, sizeof(event), 0);
    return 0;
}

int on_function__return(struct pt_regs* ctx)
{
    struct event_t event = {};
    char* filename_ptr;
    char* funcname_ptr;
    event.type = true;
    bpf_usdt_readarg(1, ctx, &filename_ptr);
    bpf_usdt_readarg(2, ctx, &funcname_ptr);
    event.tid = bpf_get_current_pid_tgid();
    bpf_probe_read_user_str(event.filename, 32, filename_ptr);
    bpf_probe_read_user_str(event.funcname, 32, funcname_ptr);
    event.time = bpf_ktime_get_ns();
    event.cpu = ((struct task_struct*)bpf_get_current_task())->cpu;
    events.ringbuf_output(&event, sizeof(event), 0);
    return 0;
}
)"
