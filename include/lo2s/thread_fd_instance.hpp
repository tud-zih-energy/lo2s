// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/types/thread.hpp>

namespace lo2s
{
// As explained in include/lo2s/monitor/posix_monitor.hpp,  this represents a unique mapping of
// {tid, fd, reuse count} to file
struct ThreadFdInstance
{
public:
    ThreadFdInstance() : thread(Thread::invalid()), fd(-1), instance(0)
    {
    }

    ThreadFdInstance(Thread thread, int fd, int instance)
    : thread(thread), fd(fd), instance(instance)
    {
    }

    friend bool operator==(const ThreadFdInstance& lhs, const ThreadFdInstance& rhs)
    {
        return lhs.thread == rhs.thread && lhs.fd == rhs.fd && lhs.instance == rhs.instance;
    }

    friend bool operator<(const ThreadFdInstance& lhs, const ThreadFdInstance& rhs)
    {
        if (lhs.thread == rhs.thread)
        {
            if (lhs.fd == rhs.fd)
            {
                return lhs.instance < rhs.instance;
            }
            return lhs.fd < rhs.fd;
        }
        return lhs.thread < rhs.thread;
    }

private:
    Thread thread;
    int fd;
    int instance;
};

} // namespace lo2s
