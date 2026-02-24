/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2022,
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

#include <lo2s/monitor/system_process_monitor.hpp>

#include <lo2s/trace/trace.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <string>

namespace lo2s::monitor
{
void SystemProcessMonitor::insert_process([[maybe_unused]] Process parent,
                                          [[maybe_unused]] Process process,
                                          [[maybe_unused]] std::string proc_name,
                                          [[maybe_unused]] bool spawn)
{
}

void SystemProcessMonitor::insert_thread([[maybe_unused]] Process process, Thread thread,
                                         std::string name, [[maybe_unused]] bool spawn)
{
    // in system monitoring, we only need to track the threads spawned from the process lo2s spawned
    // itself. Without this, these threads end up as "<unknown thread>". Sad times.
    trace_.emplace_thread(process, thread, name);
}

void SystemProcessMonitor::update_process_name([[maybe_unused]] Process process,
                                               [[maybe_unused]] const std::string& name)
{
}

void SystemProcessMonitor::exit_thread([[maybe_unused]] Thread thread)
{
}

} // namespace lo2s::monitor
