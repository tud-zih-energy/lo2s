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

#include <boost/format.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include <cstdint>
#include <ctime>

extern "C" {
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace lo2s
{

inline std::size_t get_page_size()
{
    static std::size_t page_size = sysconf(_SC_PAGESIZE);
    return page_size;
}

template <typename T>
class StringCache
{
public:
    static StringCache& instance()
    {
        static StringCache l;
        return l;
    }

    T& operator[](const std::string& name)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (elements_.count(name) == 0)
        {
            elements_.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                              std::forward_as_tuple(name));
        }
        return elements_.at(name);
    }

private:
    std::unordered_map<std::string, T> elements_;
    std::mutex mutex_;
};

inline std::string get_process_exe(pid_t pid)
{
    auto proc_filename = (boost::format("/proc/%d/exe") % pid).str();
    char exe_cstr[PATH_MAX + 1];
    auto ret = readlink(proc_filename.c_str(), exe_cstr, PATH_MAX);
    if (ret == -1)
    {
        return "<unknown>";
    }
    exe_cstr[ret] = '\0';
    return exe_cstr;
}

inline std::string get_process_cmdline(pid_t pid)
{
    auto proc_filename = (boost::format("/proc/%d/cmdline") % pid).str();
    // TODO
    return "";
}

inline std::string get_datetime()
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H-%M-%S");
    return oss.str();
}

inline std::string get_hostname()
{
    char c_hostname[HOST_NAME_MAX + 1];
    if (gethostname(c_hostname, HOST_NAME_MAX + 1))
    {
        return "<unknown hostname>";
    }
    return c_hostname;
}

inline int32_t get_task_last_cpu_id(std::istream& proc_stat)
{
    proc_stat.seekg(0);
    for (int i=0; i < 38; i++) {
        std::string ignore;
        std::getline(proc_stat, ignore, ' ');
    }
    int32_t cpu_id = -1;
    proc_stat >> cpu_id;
    return cpu_id;
}
}
