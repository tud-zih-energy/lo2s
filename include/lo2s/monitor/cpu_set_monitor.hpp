// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/monitor/scope_monitor.hpp>
#include <lo2s/types/cpu.hpp>

#include <map>

namespace lo2s::monitor
{

/**
 * Current implementation is just for all CPUs
 * TODO extend to list of CPUs
 */
class CpuSetMonitor : public MainMonitor
{
public:
    CpuSetMonitor();

    void run();

private:
    std::map<Cpu, ScopeMonitor> monitors_;
};
} // namespace lo2s::monitor
