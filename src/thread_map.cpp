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
#include <lo2s/process_info.hpp>
#include <lo2s/thread_monitor.hpp>

#include <chrono>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>

#include <cassert>

namespace lo2s
{
thread_map::~thread_map()
{
    log::debug() << "Cleaning up global thread map.";
    // I really hope noone accesses the thread_map while it's being deconstructed.
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    disable();
    try_join();

    if (threads_.empty())
    {
        return;
    }

    log::debug() << "Going for a short nap while threads are finishing. This is totally fine.";
    // TODO Sleep for sampling_period instead.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    try_join();
    if (threads_.empty())
    {
        return;
    }

    log::info() << "Waiting 1 second for all threads to finish.";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    try_join();
    if (threads_.empty())
    {
        return;
    }

    log::warn() << "Not all monitoring threads finished, giving up.";
}

process_info& thread_map::insert_process(pid_t pid, bool enable_on_exec)
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

void thread_map::insert(pid_t pid, pid_t tid, bool enable_on_exec)
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
        log::debug() << "Registered thread " << tid << ", (pid " << pid << ")";
    }
    else
    {
        log::warn() << "Thread " << tid << ", (pid " << pid << ") already registered";
    }
}

void thread_map::disable(pid_t tid)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    get_thread(tid).disable();
}

void thread_map::disable()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto& elem : threads_)
    {
        if (elem.second.enabled())
        {
            elem.second.disable();
        }
    }
}

thread_monitor& thread_map::get_thread(pid_t tid)
{
    return threads_.at(tid);
}

void thread_map::try_join()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto end = threads_.end();
    for (auto iter = threads_.begin(); iter != end;)
    {
        if (iter->second.finished())
        {
            threads_.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}
}
