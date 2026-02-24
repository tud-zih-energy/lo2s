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

#include <stdexcept>
#include <string>

#include <cassert>
#include <cstdint>

#include <fmt/format.h>

namespace lo2s
{

enum class ExecutionScopeType
{
    THREAD,
    PROCESS,
    CPU,
    UNKNOWN
};

class Thread;
class Cpu;
class Process;

// This is a wrapper around Cpus Threads and Processes. This allows us to simplify the middle-end of
// lo2s as almost all of it doesn't care if it deals with processes, threads or cpus.
class ExecutionScope
{
public:
    ExecutionScope() : type(ExecutionScopeType::UNKNOWN), id(-1)
    {
    }

    explicit ExecutionScope(Thread thread);
    explicit ExecutionScope(Process process);
    explicit ExecutionScope(Cpu cpu);

    std::string name() const
    {
        switch (type)
        {
        case ExecutionScopeType::THREAD:
            return fmt::format("thread {}", id);
        case ExecutionScopeType::PROCESS:
            return fmt::format("process {}", id);
        case ExecutionScopeType::CPU:
            return fmt::format("cpu {}", id);
        default:
            throw std::runtime_error("Unknown ExecutionScopeType!");
        }
    }

    Thread as_thread() const;

    Process as_process() const;

    Cpu as_cpu() const;

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
        return lhs.id < rhs.id;
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
    int64_t id;
};

} // namespace lo2s
