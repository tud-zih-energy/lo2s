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

#include <lo2s/monitor/threaded_monitor.hpp>

#include <lo2s/summary.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <fmt/core.h>

namespace lo2s
{
namespace monitor
{
ThreadedMonitor::ThreadedMonitor(trace::Trace& trace, const std::string& name)
: trace_(trace), name_(name), num_wakeups_(0)
{
}

ThreadedMonitor::~ThreadedMonitor()
{
    summary().record_perf_wakeups(num_wakeups_);
}

void ThreadedMonitor::start()
{
    assert(!thread_.joinable());
    thread_ = std::thread([this]() { this->thread_main(); });
}

std::string ThreadedMonitor::name() const
{
    if (name_.empty())
    {
        return group();
    }
    return fmt::format("{} ({})", group(), name_);
}

void ThreadedMonitor::thread_main()
{
    register_thread();
    initialize_thread();
    Log::debug() << name() << " starting.";
    run();
    Log::debug() << name() << " ending.";
    finalize_thread();
}

void ThreadedMonitor::register_thread()
{
    trace_.add_monitoring_thread(gettid(), name(), group());
}
} // namespace monitor
} // namespace lo2s
