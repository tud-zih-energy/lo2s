// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/threaded_monitor.hpp>

#include <lo2s/log.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <string>
#include <utility>

#include <cassert>

#include <fmt/core.h>
#include <fmt/format.h>

namespace lo2s::monitor
{
ThreadedMonitor::ThreadedMonitor(trace::Trace& trace, std::string name)
: trace_(trace), name_(std::move(name))
{
}

ThreadedMonitor::~ThreadedMonitor()
{
    summary().record_perf_wakeups(num_wakeups_);
    assert(!thread_.joinable());
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
    trace_.emplace_monitoring_thread(gettid(), name(), group());
}
} // namespace lo2s::monitor
