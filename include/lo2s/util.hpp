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

#include <lo2s/address.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/helpers/expected.hpp>
#include <lo2s/helpers/fd.hpp>
#include <lo2s/types.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <cstdint>
#include <ctime>

extern "C"
{
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
}

namespace lo2s
{
namespace memory
{
template <class T>
struct MallocDelete
{
    void operator()(T* ptr) const
    {
        free(ptr);
    }
};
} // namespace memory

template <typename T>
class BinaryCache
{
public:
    static BinaryCache& instance()
    {
        static BinaryCache l;
        return l;
    }

    std::shared_ptr<T> operator[](const std::string& name)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return elements_.try_emplace(name, std::make_shared<T>(name)).first->second;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<T>> elements_;
    std::mutex mutex_;
};

std::size_t get_page_size();
std::string get_process_exe(Process process);
std::string get_process_comm(Process process);
std::vector<std::string> get_thread_cmdline(Thread thread);
std::string get_task_comm(Process process, Thread thread);

std::string get_nec_thread_comm(Thread thread);

std::chrono::duration<double> get_cpu_time();
std::string get_datetime();

const struct ::utsname& get_uname();

template <typename T>
T get_sysctl(const std::string& group, const std::string& name)
{
    auto sysctl_path = std::filesystem::path("/proc/sys") / group / name;
    std::ifstream sysctl_stream;
    sysctl_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    sysctl_stream.open(sysctl_path);

    T result;
    sysctl_stream >> result;
    return result;
}

int32_t get_task_last_cpu_id(std::istream& proc_stat);

std::map<Process, std::map<Thread, std::string>> get_comms_for_running_threads();

void try_pin_to_scope(ExecutionScope scope);

Expected<Fd, ErrnoError> get_cgroup_mountpoint_fd(std::string cgroup);

void bump_rlimit_fd();
struct rlimit initial_rlimit_fd();

Thread gettid();

std::set<std::uint32_t> parse_list(std::string list);
std::set<std::uint32_t> parse_list_from_file(std::filesystem::path file);

bool known_non_executable(const std::string& filename);

std::map<Mapping, std::string> read_maps(Process p);

bool is_kernel_thread(Thread thread);

} // namespace lo2s
