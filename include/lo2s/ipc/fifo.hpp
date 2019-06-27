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

#pragma once

#include <lo2s/log.hpp>

#include <fstream>
#include <mutex>
#include <string>
#include <type_traits>

namespace lo2s
{
namespace ipc
{
class Fifo
{
public:
    static void create(pid_t pid, const std::string& suffix = "default");

public:
    Fifo(pid_t pid, const std::string& suffix = "default");

    template <typename T>
    void write(const T& data)
    {
        static_assert(std::is_pod<T>::value, "Can only write POD types");

        write(reinterpret_cast<const char*>(&data), sizeof(data));
    }

    void write(const std::string& data)
    {
        auto size = data.size();
        write(size);
        write(data.c_str(), data.size());
    }

    template <typename T>
    void read(T& data)
    {
        static_assert(std::is_pod<T>::value, "Can only write POD types");

        read(reinterpret_cast<char*>(&data), sizeof(T));
    }

    void read(std::string& result)
    {
        std::size_t size;
        read(size);

        if (has_data())
        {
            result.resize(size);
            read(&result[0], size);
        }
        else
        {
            throw std::runtime_error("no data available now, this looks bad. :C");
        }
    }

    bool has_data();

public:
    void write(const char* data, std::size_t size);
    void read(char* data, std::size_t size);

public:
    std::string path() const;

    static std::string path(pid_t pid, const std::string& suffix);

public:
    int fd() const
    {
        return fd_;
    }

private:
    pid_t pid_;
    std::string suffix_;
    int fd_;
    std::mutex mutex_;
};
} // namespace ipc
} // namespace lo2s
