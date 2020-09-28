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
    METRIC,
    SWITCH,
    UNKNOWN
};

struct MeasurementScope
{
    MeasurementScopeType type;
    ExecutionScope scope;

    MeasurementScope() : type(MeasurementScopeType::UNKNOWN), scope()
    {
    }

    MeasurementScope(MeasurementScopeType type, ExecutionScope s) : type(type), scope(s)
    {
    }

    static MeasurementScope sample(ExecutionScope s)
    {
        return { MeasurementScopeType::SAMPLE, s };
    }

    static MeasurementScope metric(ExecutionScope s)
    {
        return { MeasurementScopeType::METRIC, s };
    }

    static MeasurementScope context_switch(ExecutionScope s)
    {
        return { MeasurementScopeType::SWITCH, s };
    }

    friend bool operator==(const MeasurementScope& lhs, const MeasurementScope& rhs)
    {
        return (lhs.scope == rhs.scope) && lhs.type == rhs.type;
    }

    friend bool operator<(const MeasurementScope& lhs, const MeasurementScope& rhs)
    {
        if (lhs.type != rhs.type)
        {
            return lhs.type < rhs.type;
        }
        else
        {
            return lhs.scope < rhs.scope;
        }
    }

    std::string name()
    {
        switch (type)
        {
        case MeasurementScopeType::METRIC:
            return fmt::format("metrics for {}", scope.name());
        case MeasurementScopeType::SAMPLE:
            return fmt::format("samples for {}", scope.name());
        case MeasurementScopeType::SWITCH:
            return fmt::format("context switches for {}", scope.name());
        default:
            throw new std::runtime_error("Unknown ExecutionScopeType!");
        }
    }
};

} // namespace lo2s
