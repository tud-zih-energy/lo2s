// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/types/cpu.hpp>

#include <lo2s/execution_scope.hpp>

#include <cstdint>

namespace lo2s
{

int64_t Cpu::as_int() const
{
    return cpu_;
}

ExecutionScope Cpu::as_scope() const
{
    return ExecutionScope(*this);
}

std::ostream& operator<<(std::ostream& stream, const Cpu& cpu)
{
    return stream << fmt::format("{}", cpu);
}

} // namespace lo2s
