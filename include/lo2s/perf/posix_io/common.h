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
