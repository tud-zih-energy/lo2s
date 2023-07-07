/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2021,
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

#include <cassert>
#include <map>

#include <fmt/core.h>

#include <lo2s/log.hpp>
#include <lo2s/types.hpp>
namespace lo2s
{

enum class ExecutionScopeType
{
    THREAD,
    PROCESS,
    CPU,
    GPU,
    UNKNOWN
};

class Cpu;
class Process;
class Thread;
// This is a wrapper around Cpus Threads and Processes. This allows us to simplify the middle-end of
// lo2s as almost all of it doesn't care if it deals with processes, threads or cpus.
class ExecutionScope
{
public:
    ExecutionScope() : type(ExecutionScopeType::UNKNOWN), id(-1)
    {
    }

    explicit ExecutionScope(Thread thread) : type(ExecutionScopeType::THREAD), id(thread.as_pid_t())
    {
    }

    explicit ExecutionScope(Process process)
    : type(ExecutionScopeType::PROCESS), id(process.as_pid_t())
    {
    }

    explicit ExecutionScope(Cpu cpu) : type(ExecutionScopeType::CPU), id(cpu.as_int())
    {
    }
    
    explicit ExecutionScope(Gpu gpu) : type(ExecutionScopeType::GPU), id(gpu.as_int())
    {
    }

    std::string name() const
    {
        switch (type)
        {
        case ExecutionScopeType::THREAD:
            return fmt::format("thread {}", id);
        case ExecutionScopeType::PROCESS:
            return fmt::format("process {}");
        case ExecutionScopeType::CPU:
            return fmt::format("cpu {}", id);
        default:
            throw new std::runtime_error("Unknown ExecutionScopeType!");
        }
    }

    Thread as_thread() const
    {
        assert(type == ExecutionScopeType::THREAD);
        return Thread(id);
    }
    Process as_process() const
    {
        assert(type == ExecutionScopeType::PROCESS);
        return Process(id);
    }
    Cpu as_cpu() const
    {
        assert(type == ExecutionScopeType::CPU);
        return Cpu(id);
    }

    bool is_process() const
    {
        return type == ExecutionScopeType::PROCESS;
    }

    bool is_thread() const
    {
        return type == ExecutionScopeType::THREAD;
    }

    bool is_cpu() const
    {
        return type == ExecutionScopeType::CPU;
    }
    // Needed because this is used as a Key in some Structures.
    // Simply order (arbitrarly) by type first, then by scope
    friend bool operator<(const ExecutionScope& lhs, const ExecutionScope& rhs)
    {
        if (lhs.type != rhs.type)
        {
            return lhs.type < rhs.type;
        }
        else
        {
            return lhs.id < rhs.id;
        }
    }

    friend bool operator==(const ExecutionScope& lhs, const ExecutionScope& rhs)
    {
        return lhs.type == rhs.type && lhs.id == rhs.id;
    }

    friend bool operator!=(const ExecutionScope& lhs, const ExecutionScope& rhs)
    {
        return lhs.type != rhs.type || lhs.id != rhs.id;
    }

private:
    ExecutionScopeType type;
    int id;
};

// This class tracks the relation ship between locations for which we can measure things and the
// group they belong to. For Threads the location group is the process they belong to. CPUs are
// their own group (for now)
class ExecutionScopeGroup
{
public:
    static ExecutionScopeGroup& instance()
    {
        static ExecutionScopeGroup group;
        return group;
    }

    bool is_group(const ExecutionScope& scope) const
    {
        const auto& it = groups_.find(scope);
        if (it == groups_.end())
        {
            return false;
        }
        return it->second == scope;
    }

    bool is_process(const Thread& thread) const
    {
        return is_group(thread.as_scope());
    }

    ExecutionScope get_parent(const ExecutionScope& scope) const
    {
        return groups_.at(scope);
    }

    Process get_process(Thread thread) const
    {
        // If we don't know the parent process by the time we get to know the child thread, we will
        // never know it, so just report pid 0
        if (groups_.count(thread.as_scope()) == 0)
        {
            return Process(0);
        }
        return groups_.at(thread.as_scope()).as_process();
    }

    void add_process(Process process)
    {
        groups_.emplace(process.as_thread().as_scope(), process.as_scope());
    }

    void add_thread(Thread thread, Process process)
    {
        groups_.emplace(thread.as_scope(), process.as_scope());
    }

    // If we only know the parent thread, try to find the parent process.
    void add_thread(Thread child, Thread parent)
    {
        const auto& real_parent = groups_.find(parent.as_scope());
        if (real_parent == groups_.end())
        {
            // Per convention, the parent process always has to be a process, so convert here
            // accordinglt
            Log::debug() << "No parent process found for " << child << " using " << parent
                         << "as a parent instead";
            groups_.emplace(child.as_scope(), parent.as_process().as_scope());
        }
        else
        {
            groups_.emplace(child.as_scope(), real_parent->second);
        }
    }

    void add_cpu(Cpu cpu)
    {
        groups_.emplace(cpu.as_scope(), cpu.as_scope());
    }

private:
    ExecutionScopeGroup()
    {
    }

    ~ExecutionScopeGroup()
    {
    }
    std::map<ExecutionScope, ExecutionScope> groups_;
};

} // namespace lo2s
