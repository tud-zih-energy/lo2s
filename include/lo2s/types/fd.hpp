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

extern "C"
{
#include <unistd.h>
}

namespace lo2s
{

class WeakFd;
class Fd
{
public:
    explicit Fd(int fd) : fd_(fd)
    {
    }

    friend WeakFd;

    Fd(Fd&) = delete;
    Fd& operator=(Fd&) = delete;

    Fd(Fd&& other)
    {
        fd_ = other.fd_;
        other.fd_ = -1;
    }

    Fd& operator=(Fd&& other)
    {
        fd_ = other.fd_;
        other.fd_ = -1;
        return *this;
    }

    int as_int() const
    {
        return fd_;
    }

    static Fd invalid()
    {
        return Fd(-1);
    }

    bool is_valid() const
    {
        return fd_ != -1;
    }

    ~Fd()
    {
        if (fd_ != -1)
        {
            close(fd_);
        }
    }
    friend bool operator==(const WeakFd& lhs, const Fd& rhs);
    friend bool operator==(const Fd& lhs, const WeakFd& rhs);

    operator WeakFd() const;

private:
    int fd_;
};

class WeakFd
{
public:
    explicit WeakFd(int fd) : fd_(fd)
    {
    }
    friend bool operator==(const WeakFd& lhs, const WeakFd& rhs)
    {
        return lhs.fd_ == rhs.fd_;
    }

    friend bool operator!=(const WeakFd& lhs, const WeakFd& rhs)
    {
        return lhs.fd_ != rhs.fd_;
    }

    friend bool operator<(const WeakFd& lhs, const WeakFd& rhs)
    {
        return lhs.fd_ < rhs.fd_;
    }
    friend bool operator==(const WeakFd& lhs, const Fd& rhs);
    friend bool operator==(const Fd& lhs, const WeakFd& rhs);

    int as_int() const
    {
        return fd_;
    }

private:
    int fd_;
};
} // namespace lo2s
