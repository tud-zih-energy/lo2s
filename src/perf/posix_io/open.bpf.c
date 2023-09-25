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

// The generated vmlinux.h headers has to go first
// clang-format off
#include <vmlinux.h>
// clang-format on

#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <lo2s/perf/posix_io/common.h>

char LICENSE[] SEC("license") = "GPL";

struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} rb SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, 256 * 1024);
    __type(key, u32);
    __type(value, char[256]);
} open_cache SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_LRU_HASH);
    __uint(max_entries, 256 * 1024);
    __type(key, u32);
    __type(value, char);
} pids SEC(".maps");

SEC("kprobe/do_filp_open")
int BPF_KPROBE(do_filp_open, int dfd, struct filename* fn, const struct open_flags* op)
{
    u32 pid = bpf_get_current_pid_tgid();

    if (!bpf_map_lookup_elem(&pids, &pid))
        return 0;

    char name[256];
    char* name_ptr = BPF_CORE_READ(fn, name);
    bpf_probe_read_kernel_str(name, 256, name_ptr);
    bpf_map_update_elem(&open_cache, &pid, name, BPF_ANY);
    return 0;
}

SEC("tp/syscalls/sys_exit_openat")
int handle_openat_ret(struct syscalls_sys_exit_openat* ctx)
{
    if (ctx->ret < 0)
        return 0;

    u32 pid = bpf_get_current_pid_tgid();

    if (!bpf_map_lookup_elem(&pids, &pid))
        return 0;

    char* filename = bpf_map_lookup_elem(&open_cache, &pid);

    if (filename == 0)
        return 0;

    struct open_event* e;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
    if (!e)
    {
        return 0;
    }

    __builtin_memcpy(e->filename, filename, 256);
    e->header.type = OPEN;
    e->header.pid = pid;
    e->header.fd = ctx->ret;
    e->header.time = bpf_ktime_get_ns();
    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tp/syscalls/sys_enter_close")
int handle_close(struct syscalls_sys_enter_close* ctx)
{
    u32 pid = bpf_get_current_pid_tgid();

    if (!bpf_map_lookup_elem(&pids, &pid))
        return 0;

    struct posix_event_header* e;
    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);

    if (!e)
        return 0;

    e->pid = pid;
    e->time = bpf_ktime_get_ns();
    e->type = CLOSE;
    e->fd = ctx->fd;

    bpf_ringbuf_submit(e, 0);

    return 0;
}

SEC("tp/syscalls/sys_enter_read")
int handle_enter_read(struct syscalls_sys_enter_rw* ctx)
{
    u32 pid = bpf_get_current_pid_tgid();

    if (!bpf_map_lookup_elem(&pids, &pid))
        return 0;

    struct read_write_event* e;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);

    if (!e)
        return 0;
    e->header.pid = pid;
    e->header.time = bpf_ktime_get_ns();
    e->header.type = READ_ENTER;
    e->header.fd = ctx->fd;
    e->count = ctx->count;
    e->buf = ctx->buf;
    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tp/syscalls/sys_enter_write")
int handle_enter_write(struct syscalls_sys_enter_rw* ctx)
{
    u32 pid = bpf_get_current_pid_tgid();

    if (!bpf_map_lookup_elem(&pids, &pid))
        return 0;
    struct read_write_event* e;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);

    if (!e)
        return 0;
    e->header.pid = pid;
    e->header.time = bpf_ktime_get_ns();
    e->header.type = WRITE_ENTER;
    e->header.fd = ctx->fd;

    e->count = ctx->count;
    e->buf = ctx->buf;
    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tp/syscalls/sys_exit_read")
int handle_exit_read(void* ctx)
{
    u32 pid = bpf_get_current_pid_tgid();

    if (!bpf_map_lookup_elem(&pids, &pid))
        return 0;

    struct posix_event_header* e;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);

    if (!e)
        return 0;
    e->pid = pid;
    e->time = bpf_ktime_get_ns();
    e->type = READ_EXIT;

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tp/syscalls/sys_exit_write")
int handle_exit_write(void* ctx)
{
    u32 pid = bpf_get_current_pid_tgid();
    if (!bpf_map_lookup_elem(&pids, &pid))
        return 0;

    struct posix_event_header* e;

    e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);

    if (!e)
        return 0;

    e->pid = pid;
    e->time = bpf_ktime_get_ns();
    e->type = WRITE_EXIT;

    bpf_ringbuf_submit(e, 0);
    return 0;
}
