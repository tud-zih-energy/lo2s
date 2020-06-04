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

#include <lo2s/trace/fwd.hpp>

#include <string>
#include <thread>

#include <cassert>

namespace lo2s
{
namespace monitor
{
class ThreadedMonitor
{
public:
    ThreadedMonitor(trace::Trace& trace, const std::string& name);

    // We don't want copies!
    ThreadedMonitor(const ThreadedMonitor&) = delete;
    ThreadedMonitor& operator=(const ThreadedMonitor&) = delete;
    // Moving is still a bit tricky (keep moved-from in a useful state), avoid it for now.
    ThreadedMonitor(ThreadedMonitor&&) = delete;
    ThreadedMonitor& operator=(ThreadedMonitor&&) = delete;

    virtual ~ThreadedMonitor();

    virtual void start();
    virtual void stop() = 0;

    std::string name() const;

    virtual std::string group() const = 0;

protected:
    virtual void run() = 0;

    void thread_main();
    virtual void monitor() = 0;

    void register_thread();

    virtual void initialize_thread()
    {
    }

    virtual void finalize_thread()
    {
    }

protected:
    std::thread thread_;
    trace::Trace& trace_;
    std::string name_;

    std::size_t num_wakeups_;
};
} // namespace monitor
} // namespace lo2s
