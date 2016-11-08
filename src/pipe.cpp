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
#include "pipe.hpp"
#include "error.hpp"
#include "log.hpp"

#include <utility>

#include <cerrno>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
}

namespace lo2s
{

constexpr int READ_FD = 0;
constexpr int WRITE_FD = 1;

pipe::pipe()
{
    int res = ::pipe(fds_);

    if (res == -1)
    {
        throw_errno();
    }

    fd_open_[READ_FD] = true;
    fd_open_[WRITE_FD] = true;
}

pipe::~pipe()
{
    try
    {
        close();
    }
    catch (std::exception& e)
    {
        log::error() << "Caught exception in pipe destruction: " << e.what();
    }
}

pipe::pipe(pipe&& other) noexcept
{
    using std::swap;

    swap(fds_[READ_FD], other.fds_[READ_FD]);
    swap(fd_open_[READ_FD], other.fd_open_[READ_FD]);

    swap(fds_[WRITE_FD], other.fds_[WRITE_FD]);
    swap(fd_open_[WRITE_FD], other.fd_open_[WRITE_FD]);
}

pipe& pipe::operator=(pipe&& other) noexcept
{
    using std::swap;

    swap(fds_[READ_FD], other.fds_[READ_FD]);
    swap(fd_open_[READ_FD], other.fd_open_[READ_FD]);

    swap(fds_[WRITE_FD], other.fds_[WRITE_FD]);
    swap(fd_open_[WRITE_FD], other.fd_open_[WRITE_FD]);

    return *this;
}

std::size_t pipe::write(const void* buf, size_t count)
{
    if (!fd_open_[WRITE_FD])
    {
        throw std::system_error(EINVAL, std::system_category());
    }

    ssize_t res = ::write(fds_[WRITE_FD], buf, count);

    if (res == -1)
    {
        throw_errno();
    }

    return static_cast<std::size_t>(res);
}

std::size_t pipe::write()
{
    char buf = 0;
    return write(&buf, 1);
}

std::size_t pipe::read(void* buf, size_t count)
{
    if (!fd_open_[READ_FD])
    {
        throw std::system_error(EINVAL, std::system_category());
    }

    ssize_t res = ::read(fds_[READ_FD], buf, count);

    if (res == -1)
    {
        throw_errno();
    }

    return static_cast<std::size_t>(res);
}

std::size_t pipe::read()
{
    char buf;
    return read(&buf, 1);
}

int pipe::read_fd() const
{
    if (!fd_open_[READ_FD])
    {
        throw std::system_error(EINVAL, std::system_category());
    }

    return fds_[READ_FD];
}
int pipe::write_fd() const
{
    if (!fd_open_[WRITE_FD])
    {
        throw std::system_error(EINVAL, std::system_category());
    }

    return fds_[WRITE_FD];
}

void pipe::fd_flags(std::size_t fd, int flags)
{
    if (!fd_open_[fd])
    {
        throw std::system_error(EINVAL, std::system_category());
    }

    int res = fcntl(fds_[fd], F_SETFD, flags);

    if (res == -1)
    {
        throw_errno();
    }
}

void pipe::read_fd_flags(int flags)
{
    fd_flags(READ_FD, flags);
}

void pipe::write_fd_flags(int flags)
{
    fd_flags(WRITE_FD, flags);
}

void pipe::close()
{
    close_read_fd();
    close_write_fd();
}

void pipe::close_fd(std::size_t fd)
{
    if (fd_open_[fd])
    {
        int res = ::close(fds_[fd]);

        if (res == -1)
        {
            throw_errno();
        }

        fd_open_[fd] = false;
    }
}

void pipe::close_read_fd()
{
    close_fd(READ_FD);
}
void pipe::close_write_fd()
{
    close_fd(WRITE_FD);
}
}
