// SPDX-FileCopyrightText: 2016-2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/execution_scope_group.hpp>
#include <lo2s/monitor/abstract_process_monitor.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <string>

#include <cstddef>

extern "C"
{
#include <signal.h>
}

namespace lo2s
{

class ProcessController
{
    enum class SignalHandlingState
    {
        KeepRunning,
        Stop
    };

public:
    ProcessController(Process child, const std::string& name, bool spawn,
                      monitor::AbstractProcessMonitor& monitor);

    ProcessController(ProcessController&) = delete;
    ProcessController(ProcessController&&) = delete;
    ProcessController& operator=(ProcessController&) = delete;
    ProcessController& operator=(ProcessController&&) = delete;
    ~ProcessController();

    void run();

private:
    void handle_ptrace_event(Thread child, int event);

    SignalHandlingState handle_signal(Thread child, int status);
    SignalHandlingState handle_stop_signal(Thread child, int status);

    Thread first_child_;
    sighandler_t default_signal_handler;
    monitor::AbstractProcessMonitor& monitor_;
    std::size_t num_wakeups_{ 0 };
    ExecutionScopeGroup& groups_;
};
} // namespace lo2s
