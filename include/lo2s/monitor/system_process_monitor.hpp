// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <lo2s/monitor/abstract_process_monitor.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <string>

namespace lo2s::monitor
{

class SystemProcessMonitor : public AbstractProcessMonitor
{
public:
    SystemProcessMonitor(lo2s::trace::Trace& trace) : trace_(trace)
    {
    }

    void insert_process(Process parent, Process process, std::string proc_name,
                        bool spawn) override;

    void insert_thread(Process process, Thread thread, std::string name, bool spawn) override;

    void exit_thread(Thread thread) override;

    void update_process_name(Process process, const std::string& name) override;

private:
    lo2s::trace::Trace& trace_;
};
} // namespace lo2s::monitor
