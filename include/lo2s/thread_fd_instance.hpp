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

#include <lo2s/types.hpp>

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
