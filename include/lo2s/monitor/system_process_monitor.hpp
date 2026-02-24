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
