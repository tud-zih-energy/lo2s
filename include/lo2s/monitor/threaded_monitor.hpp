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

#include <thread>

#include <cassert>

namespace lo2s
{
namespace monitor
{
class ThreadedMonitor
{
public:
    ThreadedMonitor() = default;

    // We don't want copies. Should be implicitly deleted due to unique_ptr
    ThreadedMonitor(const ThreadedMonitor&) = delete;
    ThreadedMonitor& operator=(const ThreadedMonitor&) = delete;
    // Moving is still a bit tricky (keep moved-from in a useful state), avoid it for now.
    ThreadedMonitor(ThreadedMonitor&&) = delete;
    ThreadedMonitor& operator=(ThreadedMonitor&&) = delete;

    virtual ~ThreadedMonitor()
    {
        assert(!thread_.joinable());
    }

    virtual void start()
    {
        assert(!thread_.joinable());
        thread_ = std::thread([this]() { this->run(); });
    }
    virtual void stop() = 0;

protected:
    virtual void run() = 0;
    virtual void monitor() = 0;

    virtual void initialize_thread()
    {
    }
    virtual void finalize_thread()
    {
    }

protected:
    std::thread thread_;
};
}
}
