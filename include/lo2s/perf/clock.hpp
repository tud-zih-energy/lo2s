// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

// All the time stuff is based on the assumption that all times are nanoseconds.
// XXX check if we need this exposed or can just move it to time/converter.hpp
namespace lo2s::perf
{

struct Clock
{
    // This is except from the type = CamelCase naming convention to be compatible
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<Clock, duration>;
};
} // namespace lo2s::perf
