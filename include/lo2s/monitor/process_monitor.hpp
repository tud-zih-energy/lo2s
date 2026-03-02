// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
