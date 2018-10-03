/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016-2018,
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

#include <map>
#include <string>

extern "C"
{
#include <signal.h>
}

namespace lo2s
{
/** A container mapping thread IDs to the PIDs of the processes a thread was
 *  launched in.  In the language of the Linux kernel, this container maps
 *  process identifiers (PIDs) to the corresponding task group IDs (TGIDs).
 *  When referring to 'main thread', we are talking about the thread in a
 *  process where tid == pid.
 **/
class ThreadToProcessMapping
{
public:
    ThreadToProcessMapping() = default;

    /** Add a the main thread of a process by PID.
     *
     *  This is equivalent to add_thread_to_process(\p pid, \p pid), as a
     *  process is just a single thread where tid == pid.
     **/
    void add_main_thread_for_process(pid_t pid)
    {
        tid_to_pid_.emplace(pid, pid);
    }

    /** Add a thread identified by \p tid to the thread group of \p pid.
     **/
    void add_thread_to_process(pid_t tid, pid_t pid)
    {
        tid_to_pid_.emplace(tid, pid);
    }

    /** Retrieve the PID of the process that thread \p tid is contained in.
     **/
    pid_t get_process_for_thread(pid_t tid) const
    {
        return tid_to_pid_.at(tid);
    }

    /** Remove thread \p tid from the mapping.
     **/
    void remove_thread(pid_t tid)
    {
        tid_to_pid_.erase(tid);
    }

private:
    std::map<pid_t, pid_t> tid_to_pid_;
};

class ProcessController
{
public:
    ProcessController(pid_t child, const std::vector<std::string>& command_line, bool spawn,
                      monitor::AbstractProcessMonitor& monitor);

    ~ProcessController();

    void run();

private:
    void handle_ptrace_event(pid_t child, int event);

    void handle_signal(pid_t child, int status);

    const pid_t first_child_;
    sighandler_t default_signal_handler;
    monitor::AbstractProcessMonitor& monitor_;
    std::size_t num_wakeups_;

    ThreadToProcessMapping watched_threads_; //!< Keep track of which threads we
                                             //!< are watching via ptrace and to
                                             //!< which processes they belong
};
} // namespace lo2s
