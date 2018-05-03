/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

#include <lo2s/monitor/fwd.hpp>

#include <lo2s/monitor/threaded_monitor.hpp>

#include <lo2s/dtrace/types.hpp>
#include <lo2s/trace/fwd.hpp>

#include <atomic>
#include <string>
#include <thread>

namespace lo2s
{
namespace monitor
{

/**
 * Lifetime of an IntervalMonitor
 *
 * [created]
 *
 * start()
 *
 * [running]
 *
 * stop()
 *
 * If the thread was just waiting, the condition variable will instantly stop it.
 * If the thread is just doing its monitoring, it will break out of the the loop afterwards
 * The thread will then be joined and the IntervalMonitor will be finished()
 *
 * ~IntervalMonitor()
 *
 * The destructor won't do anything. Calling it before stop(), will stop it but is considered an
 * error.
 */
class DtraceSleepMonitor : public ThreadedMonitor
{
public:
    DtraceSleepMonitor(trace::Trace& trace, const std::string& name);
    virtual ~DtraceSleepMonitor();

    void stop() override;

protected:
    void run() override;

private:
    std::atomic<bool> stop_requested_;

protected:
    lo2s::dtrace::Handle handle;
};
} // namespace monitor
} // namespace lo2s
