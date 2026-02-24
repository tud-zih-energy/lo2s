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
#include <lo2s/monitor/abstract_process_monitor.hpp>
#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/types/process.hpp>
#ifdef HAVE_BPF
#include <lo2s/monitor/posix_monitor.hpp>
#endif
#include <lo2s/monitor/scope_monitor.hpp>

#include <map>
#include <string>

namespace lo2s::monitor
{

class ProcessMonitor : public AbstractProcessMonitor, public MainMonitor
{
public:
    ProcessMonitor();

    ProcessMonitor(ProcessMonitor&) = delete;
    ProcessMonitor& operator=(ProcessMonitor&) = delete;

    ProcessMonitor(ProcessMonitor&&) = delete;
    ProcessMonitor& operator=(ProcessMonitor&&) = delete;

    ~ProcessMonitor() override;
    void insert_process(Process parent, Process child, std::string proc_name,
                        bool spawn = false) override;
    void insert_thread(Process parent, Thread child, std::string name, bool spawn = false) override;

    void exit_thread(Thread thread) override;

    void update_process_name(Process process, const std::string& name) override;

private:
    std::map<Thread, ScopeMonitor> threads_;
#ifdef HAVE_BPF
    std::unique_ptr<PosixMonitor> posix_monitor_;
#endif
};
} // namespace lo2s::monitor
