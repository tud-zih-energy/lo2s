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

#include <lo2s/execution_scope.hpp>

namespace lo2s
{

enum class MeasurementScopeType
{
    SAMPLE,
    GROUP_METRIC,
    USERSPACE_METRIC,
    NEC_METRIC,
    BIO,
    SYSCALL,
    RB,
    TRACEPOINT,
    UNKNOWN
};

struct MeasurementScope
{
    MeasurementScopeType type;
    ExecutionScope scope;
    std::string rb_name = "";

    MeasurementScope() : type(MeasurementScopeType::UNKNOWN), scope()
    {
    }

    MeasurementScope(MeasurementScopeType type, ExecutionScope s, std::string name = "")
    : type(type), scope(s), rb_name(name)
    {
    }

    static MeasurementScope sample(ExecutionScope s)
    {
        return { MeasurementScopeType::SAMPLE, s };
    }

    static MeasurementScope group_metric(ExecutionScope s)
    {
        return { MeasurementScopeType::GROUP_METRIC, s };
    }

    static MeasurementScope nec_metric(ExecutionScope s)
    {
        return { MeasurementScopeType::NEC_METRIC, s };
    }

    static MeasurementScope userspace_metric(ExecutionScope s)
    {
        return { MeasurementScopeType::USERSPACE_METRIC, s };
    }

    static MeasurementScope bio(ExecutionScope s)
    {
        return { MeasurementScopeType::BIO, s };
    }

    static MeasurementScope syscall(ExecutionScope s)
    {
        return { MeasurementScopeType::SYSCALL, s };
    }

    static MeasurementScope rb(ExecutionScope s, std::string name)
    {
        return { MeasurementScopeType::RB, s, name };
    }

    static MeasurementScope tracepoint(ExecutionScope s)
    {
        return { MeasurementScopeType::TRACEPOINT, s };
    }

    friend bool operator==(const MeasurementScope& lhs, const MeasurementScope& rhs)
    {
        if (lhs.type == MeasurementScopeType::RB && rhs.type == MeasurementScopeType::RB)
        {
            return lhs.scope == rhs.scope && lhs.rb_name == rhs.rb_name;
        }

        return (lhs.scope == rhs.scope) && lhs.type == rhs.type;
    }

    MeasurementScope from_ex_scope(ExecutionScope new_scope)
    {
        return MeasurementScope(type, new_scope, rb_name);
    }

    friend bool operator<(const MeasurementScope& lhs, const MeasurementScope& rhs)
    {
        if (lhs.type == MeasurementScopeType::RB && rhs.type == MeasurementScopeType::RB)
        {
            if (lhs.scope == rhs.scope)
            {
                return lhs.rb_name < rhs.rb_name;
            }
            else
            {
                return lhs.scope < rhs.scope;
            }
        }
        if (lhs.type == rhs.type)
        {
            return lhs.scope < rhs.scope;
        }
        else
        {
            return lhs.type < rhs.type;
        }
    }

    std::string name() const
    {
        switch (type)
        {
        case MeasurementScopeType::GROUP_METRIC:
        case MeasurementScopeType::USERSPACE_METRIC:
            return fmt::format("metrics for {}", scope.name());
        case MeasurementScopeType::NEC_METRIC:
            return fmt::format("metrics for NEC {}", scope.name());
        case MeasurementScopeType::SAMPLE:
            return fmt::format("samples for {}", scope.name());
        case MeasurementScopeType::BIO:
            return fmt::format("block layer I/O events for {}", scope.name());
        case MeasurementScopeType::SYSCALL:
            return fmt::format("syscall events for {}", scope.name());
        case lo2s::MeasurementScopeType::RB:
            return fmt::format("{} events for {}", rb_name, scope.name());
        case MeasurementScopeType::TRACEPOINT:
            return fmt::format("tracepoint events for {}", scope.name());
        default:
            throw new std::runtime_error("Unknown ExecutionScopeType!");
        }
    }
};

} // namespace lo2s
