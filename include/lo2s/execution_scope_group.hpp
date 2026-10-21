// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/log.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <map>

namespace lo2s
{

// This class tracks the relation ship between locations for which we can measure things and the
// group they belong to. For Threads the location group is the process they belong to. CPUs are
// their own group (for now)
class ThreadGroup
{
public:
    static ThreadGroup& instance()
    {
        static ThreadGroup group;
        return group;
    }

    // Checks if the given thread is a process per our tracking
    bool is_process(const Thread& thread) const
    {
        return groups_.count(thread) && groups_.at(thread).as_thread() == thread;
    }

    /*
     * A process is its own group
     */
    void add_process(Process process)
    {
        groups_.emplace(process.as_thread(), process);
    }

    void add_thread(Thread thread, Process process)
    {
        groups_.emplace(thread, process);
    }

    Process get_process(Thread thread)
    {
        if (groups_.count(thread) != 0)
        {
            return groups_.at(thread);
        }

        return Process::no_parent();
    }

    ThreadGroup(ThreadGroup&) = delete;
    ThreadGroup(ThreadGroup&&) = delete;
    ThreadGroup& operator=(ThreadGroup&) = delete;
    ThreadGroup& operator=(ThreadGroup&&) = delete;

private:
    ThreadGroup()
    {
    }

    ~ThreadGroup() = default;

    std::map<Thread, Process> groups_;
};
} // namespace lo2s
