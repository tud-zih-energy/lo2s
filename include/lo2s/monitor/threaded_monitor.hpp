// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/trace/fwd.hpp>

#include <string>
#include <thread>

#include <cstddef>

namespace lo2s::monitor
{
class ThreadedMonitor
{
public:
    ThreadedMonitor(trace::Trace& trace, std::string name);

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

    void register_thread();

    virtual void initialize_thread()
    {
    }

    virtual void finalize_thread()
    {
    }

    std::thread thread_;
    trace::Trace& trace_;
    std::string name_;

    std::size_t num_wakeups_ = 0;
};
} // namespace lo2s::monitor
