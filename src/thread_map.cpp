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

#include <lo2s/thread_map.hpp>

#include <lo2s/log.hpp>
#include <lo2s/monitor.hpp>
#include <lo2s/monitor/thread_monitor.hpp>
#include <lo2s/process_info.hpp>

#include <chrono>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>

#include <cassert>

namespace lo2s
{
ThreadMap::~ThreadMap()
{
    Log::debug() << "Cleaning up global thread map.";
    stop_all();
}

ProcessInfo& ThreadMap::insert_process(pid_t pid, bool enable_on_exec)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // TODO wait for C++17 and replace with try_emplace
    if (processes_.count(pid) == 0)
    {
        auto ret = processes_.emplace(std::piecewise_construct, std::forward_as_tuple(pid),
                                      std::forward_as_tuple(pid, enable_on_exec));
        assert(ret.second);
        return ret.first->second;
    }
    return processes_.at(pid);
}

void ThreadMap::insert(pid_t pid, pid_t tid, bool enable_on_exec)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto& info = insert_process(pid, enable_on_exec);
    if (threads_.count(tid) == 0)
    {
        // try
        {
            auto ret = threads_.emplace(
                std::piecewise_construct, std::forward_as_tuple(tid),
                std::forward_as_tuple(pid, tid, parent_monitor_, info, enable_on_exec));
            assert(ret.second);
            (void)ret;
        }
        /* Disabled for now as this will kill us later
    catch (const std::exception& e)
    {
        log::warn() << "Unable to initialize performance counters for thread " << tid
                    << ", thread " << tid << ". This thread will not be sampled.";
    }
         */
        Log::debug() << "Registered thread " << tid << ", (pid " << pid << ")";
    }
    else
    {
        Log::warn() << "Thread " << tid << ", (pid " << pid << ") already registered";
    }
}

void ThreadMap::stop(pid_t tid)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    get_thread(tid).stop();
    threads_.erase(tid);
}

void ThreadMap::stop_all()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto& elem : threads_)
    {
        elem.second.stop();
    }
    threads_.clear();
}

monitor::ThreadMonitor& ThreadMap::get_thread(pid_t tid)
{
    return threads_.at(tid);
}
}
