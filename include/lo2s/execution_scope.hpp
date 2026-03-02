// SPDX-FileCopyrightText: 2021 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
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
