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
#ifndef X86_RECORD_OTF2_PIPE_HPP
#define X86_RECORD_OTF2_PIPE_HPP

#include <cstdint>

namespace lo2s
{
class pipe
{
public:
    pipe();
    ~pipe();

    pipe(const pipe&) = delete;
    pipe& operator=(const pipe&) = delete;

    pipe(pipe&&) noexcept;
    pipe& operator=(pipe&&) noexcept;

    void read_fd_flags(int flags);
    void write_fd_flags(int flags);

    int read_fd() const;
    int write_fd() const;

    std::size_t read();
    std::size_t read(void* buf, std::size_t count);
    std::size_t write();
    std::size_t write(const void* buf, std::size_t count);

    void close();
    void close_read_fd();
    void close_write_fd();

private:
    void fd_flags(std::size_t fd, int flags);
    void close_fd(std::size_t fd);

    int fds_[2];
    bool fd_open_[2];
};
}

#endif // X86_RECORD_OTF2_PIPE_HPP
