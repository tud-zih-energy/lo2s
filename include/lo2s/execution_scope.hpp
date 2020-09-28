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

#include <map>

#include <fmt/core.h>
namespace lo2s
{

enum class ExecutionScopeType
{
    THREAD,
    CPU,
    UNKNOWN
};

struct ExecutionScope
{
    ExecutionScopeType type;
    int id;

    ExecutionScope() : type(ExecutionScopeType::UNKNOWN), id(-1)
    {
    }

    ExecutionScope(ExecutionScopeType t, int l) : type(t), id(l)
    {
    }

    static ExecutionScope cpu(int cpuid)
    {
        return { ExecutionScopeType::CPU, cpuid };
    }

    static ExecutionScope thread(pid_t pid)
    {
        return { ExecutionScopeType::THREAD, pid };
    }

    std::string name() const
    {
        switch (type)
        {
        case ExecutionScopeType::THREAD:
            return fmt::format("thread {}", id);
        case ExecutionScopeType::CPU:
            return fmt::format("cpu {}", id);
        default:
            throw new std::runtime_error("Unknown ExecutionScopeType!");
        }
    }

    pid_t tid() const
    {
        return (type == ExecutionScopeType::THREAD ? id : -1);
    }
    int cpuid() const
    {
        return (type == ExecutionScopeType::CPU ? id : -1);
    }

    // Needed becausse this is used as a Key in some Structures.
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
};

class ExecutionScopeGroup
{
public:
    static ExecutionScopeGroup& instance()
    {
        static ExecutionScopeGroup group;
        return group;
    }

    bool is_group(const ExecutionScope& scope)
    {
        const auto& it = groups_.find(scope);
        if (it == groups_.end())
        {
            return false;
        }
        return it->second == scope;
    }

    ExecutionScope get_group(const ExecutionScope& scope)
    {
        return groups_.at(scope);
    }

    void add_child(ExecutionScope child, ExecutionScope parent)
    {
        const auto& real_parent = groups_.find(parent);
        if (real_parent == groups_.end())
        {
            groups_.emplace(child, parent);
        }
        else
        {
            groups_.emplace(child, real_parent->second);
        }
    }

    void add_parent(ExecutionScope parent)
    {
        groups_.emplace(parent, parent);
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
