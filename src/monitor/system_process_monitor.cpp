// SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
