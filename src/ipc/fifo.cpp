/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2019,
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

#include <lo2s/ipc/fifo.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>

#include <cassert>

extern "C"
{
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace lo2s
{
namespace ipc
{

void Fifo::create(pid_t pid, const std::string& suffix)
{
    check_errno(mkfifo(path(pid, suffix).c_str(), 0666));
}

Fifo::Fifo(pid_t pid, const std::string& suffix) : pid_(pid), suffix_(suffix)
{
    fd_ = open(path().c_str(), O_RDWR);
    if (fd_ == -1)
    {
        Log::error() << "Failed to open fifo (" << path() << ")";
        throw_errno();
    }
}

void Fifo::write(const char* data, std::size_t size)
{
    auto res = ::write(fd_, data, size);

    if (res == -1)
    {
        throw_errno();
    }

    assert(res == size);
}

void Fifo::read(char* data, std::size_t size)
{
    auto res = ::read(fd_, data, size);

    if (res == -1)
    {
        throw_errno();
    }

    assert(res == size);
}

bool Fifo::has_data()
{
    pollfd pfd;

    pfd.fd = fd_;
    pfd.events = POLLIN;

    auto res = ::poll(&pfd, 1, 1);

    if (res == -1)
    {
        throw_errno();
    }

    return res != 0;
}

std::string Fifo::path() const
{
    return path(pid_, suffix_);
}

std::string Fifo::path(pid_t pid, const std::string& suffix)
{
    return "/tmp/lo2s-Fifo-" + std::to_string(pid) + "-" + suffix;
}
} // namespace ipc

} // namespace lo2s
