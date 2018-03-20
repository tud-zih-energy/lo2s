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
#include <lo2s/process_map/process_map.hpp>
#include <lo2s/process_map/process.hpp>
#include <lo2s/process_map/thread.hpp>

#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/monitor/dummy_monitor.hpp>

#include <string>

extern "C" {
#include <sys/types.h>
}

namespace lo2s
{
namespace monitor
{

class ProcessMonitor : public DummyMonitor, public MainMonitor
{
public:
    ProcessMonitor();
    ~ProcessMonitor();

    void insert_process(pid_t pid, std::string proc_name) override;
    void insert_first_process(pid_t pid, std::string proc_name,
            bool spawn) override;
    void insert_thread(pid_t pid, pid_t tid) override;

    void exit_process(pid_t pid, std::string name) override;
    void exit_thread(pid_t tid) override;
};
}
}
