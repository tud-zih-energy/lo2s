/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016-2018,
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

#include <lo2s/execution_scope.hpp>
#include <lo2s/monitor/abstract_process_monitor.hpp>

#include <map>
#include <string>

extern "C"
{
#include <signal.h>
}

namespace lo2s
{

class ProcessController
{
public:
    ProcessController(pid_t child, const std::string& name, bool spawn,
                      monitor::AbstractProcessMonitor& monitor);

    ~ProcessController();

    void run();

private:
    void handle_ptrace_event(pid_t child, int event);

    void handle_signal(pid_t child, int status);

    const pid_t first_child_;
    sighandler_t default_signal_handler;
    monitor::AbstractProcessMonitor& monitor_;
    std::size_t num_wakeups_;
    ExecutionScopeGroup& groups_;
};
} // namespace lo2s
