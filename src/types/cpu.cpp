// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/types/cpu.hpp>

#include <iostream>

#include <cstdint>

#include <fmt/format.h>
namespace lo2s
{

int64_t Cpu::as_int() const
{
    return cpu_;
}

std::ostream& operator<<(std::ostream& stream, const Cpu& cpu)
{
    return stream << fmt::format("{}", cpu);
}

} // namespace lo2s
