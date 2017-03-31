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

#include <lo2s/monitor/main_monitor.hpp>

#include <lo2s/monitor_config.hpp>
#include <lo2s/thread_map.hpp>

#include <string>

extern "C" {
#include <signal.h>
}

namespace lo2s
{
namespace monitor
{
class ProcessMonitor : public MainMonitor
{
public:
    ProcessMonitor(const MonitorConfig& config, pid_t child, const std::string& name, bool spawn);

    ~ProcessMonitor();

    void run() override;

private:
    void handle_ptrace_event(pid_t child, int event);

    void handle_signal(pid_t child, int status);

    const pid_t first_child_;
    ThreadMap threads_;
    sighandler_t default_signal_handler;
};
}
}
