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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

extern "C" {
#include <sys/types.h>
#include <unistd.h>
}

namespace lo2s
{

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
std::chrono::duration<double> get_cpu_time();
pid_t get_max_pid();
std::size_t get_page_size();
std::string get_process_exe(pid_t pid);
std::pair<double, double> get_process_times();
std::string get_datetime();

template <typename T>
T get_sysctl(const std::string& group, const std::string& name)
{
    auto sysctl_path = boost::filesystem::path("/proc/sys") / group / name;
    boost::filesystem::ifstream sysctl_stream;
    sysctl_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    sysctl_stream.open(sysctl_path);

    T result;
    sysctl_stream >> result;
    return result;
}

int32_t get_task_last_cpu_id(std::istream& proc_stat);

std::unordered_map<pid_t, std::string> read_all_tid_exe();

void try_pin_to_cpu(int cpu, pid_t pid = 0);

pid_t gettid();
}
