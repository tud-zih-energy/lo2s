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
#include <lo2s/monitor/thread_monitor.hpp>
#include <lo2s/process_info.hpp>

#include <map>
#include <string>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{
namespace monitor
{

class ProcessMonitor : public AbstractProcessMonitor, public MainMonitor
{
public:
    ProcessMonitor();
    ~ProcessMonitor();
    void insert_process(pid_t pid, pid_t ppid, std::string proc_name, bool spawn = false) override;
    void insert_thread(pid_t pid, pid_t tid, std::string name, bool spawn = false) override;

    void exit_process(pid_t pid) override;
    void exit_thread(pid_t tid) override;

private:
    std::map<pid_t, ThreadMonitor> threads_;
};
} // namespace monitor
} // namespace lo2s
