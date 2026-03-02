// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// NOLINTBEGIN(cppcoreguidelines-use-enum-class)
enum type
{
    OPEN,
    CLOSE,
    READ_ENTER,
    READ_EXIT,
    WRITE_ENTER,
    WRITE_EXIT
};

// NOLINTEND(cppcoreguidelines-use-enum-class)

struct syscalls_sys_enter_openat
{
    unsigned long syscall_nr;
    char padding[4];
    unsigned long long dfd;
    char* filename;
    unsigned long long flags;
    unsigned long long mode;
};

struct syscalls_sys_exit_openat
{
    long syscall_nr;
    char padding[4];
    long ret;
};

struct syscalls_sys_enter_rw
{
    long syscall_nr;
    char padding0[4];
    unsigned long long fd;
    unsigned long long buf;
    unsigned long long count;
};

struct syscalls_sys_enter_close
{
    long syscall_nr;
    char padding0[4];
    unsigned long long fd;
};

struct posix_event_header
{
    int type;
    unsigned long long time;
    int pid;
    int fd;
};

struct open_event
{
    struct posix_event_header header;
    char filename[256];
};

struct read_write_event
{
    struct posix_event_header header;
    long long buf;
    long long count;
};
