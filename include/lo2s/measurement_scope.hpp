// SPDX-FileCopyrightText: 2021 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <lo2s/execution_scope.hpp>

#include <stdexcept>
#include <string>

#include <fmt/format.h>

namespace lo2s
{

enum class MeasurementScopeType
{
    SAMPLE,
    NEC_SAMPLE,
    GROUP_METRIC,
    USERSPACE_METRIC,
    NEC_METRIC,
    BIO,
    SYSCALL,
    GPU,
    TRACEPOINT,
    POSIX_IO,
    OPENMP,
    UNKNOWN
};

struct MeasurementScope
{
    MeasurementScopeType type;
    ExecutionScope scope;

    MeasurementScope() : type(MeasurementScopeType::UNKNOWN)
    {
    }

    MeasurementScope(MeasurementScopeType type, ExecutionScope s) : type(type), scope(s)
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

    static MeasurementScope gpu(ExecutionScope s)
    {
        return { MeasurementScopeType::GPU, s };
    }

    static MeasurementScope openmp(ExecutionScope s)
    {
        return { MeasurementScopeType::OPENMP, s };
    }

    static MeasurementScope tracepoint(ExecutionScope s)
    {
        return { MeasurementScopeType::TRACEPOINT, s };
    }

    static MeasurementScope posix_io(ExecutionScope s)
    {
        return { MeasurementScopeType::POSIX_IO, s };
    }

    static MeasurementScope nec_sample(ExecutionScope s)
    {
        return { MeasurementScopeType::NEC_SAMPLE, s };
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
        return lhs.scope < rhs.scope;
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
        case lo2s::MeasurementScopeType::GPU:
            return fmt::format("gpu kernel events for {}", scope.name());
        case MeasurementScopeType::TRACEPOINT:
            return fmt::format("tracepoint events for {}", scope.name());
        case MeasurementScopeType::POSIX_IO:
            return fmt::format("POSIX I/O events for {}", scope.name());
        case MeasurementScopeType::NEC_SAMPLE:
            return fmt::format("samples for NEC process {}", scope.name());
        case MeasurementScopeType::OPENMP:
            return fmt::format("OpenMP events for {}", scope.name());
        default:
            throw std::runtime_error("Unknown ExecutionScopeType!");
        }
    }
};

} // namespace lo2s
