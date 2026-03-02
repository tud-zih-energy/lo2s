// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <string>

namespace lo2s::monitor
{

class AbstractProcessMonitor
{
public:
    AbstractProcessMonitor() = default;
    AbstractProcessMonitor(AbstractProcessMonitor&) = default;
    AbstractProcessMonitor(AbstractProcessMonitor&&) = default;

    AbstractProcessMonitor& operator=(AbstractProcessMonitor const&) = default;
    AbstractProcessMonitor& operator=(AbstractProcessMonitor&&) = default;

    virtual void insert_process(Process parent, Process process, std::string proc_name,
                                bool spawn = false) = 0;
    virtual void insert_thread(Process process, Thread thread, std::string name = "",
                               bool spawn = false) = 0;

    virtual void exit_thread(Thread thread) = 0;

    virtual void update_process_name(Process process, const std::string& name) = 0;

    virtual ~AbstractProcessMonitor() = default;
};
} // namespace lo2s::monitor
