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
#include <optional>
#include <utility>

extern "C"
{
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
}

namespace lo2s
{

class Fd
{
public:
    explicit Fd(int fd) : fd_(fd)
    {
    }

    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;

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

    template <typename... Args>
    int ioctl(unsigned long request, Args&&... args)
    {
        return ::ioctl(fd_, request, std::forward<Args>(args)...);
    }

    template <typename... Args>
    int fcntl(unsigned long cmd, Args&&... args)
    {
        return ::fcntl(fd_, cmd, std::forward<Args>(args)...);
    }

    static std::optional<Fd> open(const char* pathname, int flags, mode_t mode = 0)
    {
        int fd = ::open(pathname, flags, mode);

        if (fd == -1)
        {
            return std::optional<Fd>();
        }
        else
        {
            return std::make_optional<Fd>(fd);
        }
    }

    ~Fd()
    {
        close(fd_);
    }

private:
    int fd_;
};

} // namespace lo2s
