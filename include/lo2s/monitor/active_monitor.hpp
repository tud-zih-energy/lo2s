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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace lo2s
{
class Monitor;

namespace monitor
{

/**
 * Lifetime of an ActiveMonitor
 *
 * [created]
 *
 * start()
 *
 * [running]
 *
 * stop()
 *
 * [stop_requested]
 *
 * If the thread was just waiting, the condition variable will instantly stop it.
 * If the thread is just doing its monitoring, it will break out of the the loop afterwards
 * After the thread leaves the monitoring loop, it will set finished
 *
 * [finished]
 *
 * Currently the stop itself and the monitoring action is protected by a lock, so you stop will actually block until
 * monitoring is dun and thus it will be finished very quickly after the stop call. If it ever comes to a situation
 * where this lock becomes a bottleneck, the lock can easily be moved to just around the wait_until, but then finished()
 * may be delayed a bit after the stop()
 *
 * The thread is currently joined in the destructor.
 * You should only destruct the ActiveMonitor once it is finished() to avoid waiting for the join.
 */
class ActiveMonitor
{
public:
    ActiveMonitor(Monitor& parent_monitor, std::chrono::nanoseconds interval);
    virtual ~ActiveMonitor();

    // We don't want copies. Should be implicitly deleted due to unique_ptr
    ActiveMonitor(const ActiveMonitor&) = delete;
    ActiveMonitor& operator=(const ActiveMonitor&) = delete;
    // Moving is still a bit tricky (keep moved-from in a useful state), avoid it for now.
    ActiveMonitor(ActiveMonitor&&) = delete;
    ActiveMonitor& operator=(ActiveMonitor&&) = delete;

    void start();
    void stop();

    bool finished() const
    {
        return finished_.load();
    }

    const std::string& name()
    {
        return name_;
    };

private:
    void run();

    virtual void initialize_thread()
    {
    }
    virtual void finalize_thread()
    {
    }

    virtual void monitor() = 0;

public:
    Monitor& parent_monitor()
    {
        return parent_monitor_;
    }

private:
    Monitor& parent_monitor_;
    bool stop_requested_ = false;
    std::atomic<bool> finished_{ false };

    std::mutex control_mutex_;
    std::condition_variable control_condition_;

    std::thread thread_;
    std::chrono::nanoseconds interval_;

    std::string name_;
};
}
}
