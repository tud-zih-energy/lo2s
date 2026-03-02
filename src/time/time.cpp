// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/time/time.hpp>

#include <string>

namespace lo2s::time
{
clockid_t Clock::clockid_ = CLOCK_MONOTONIC_RAW;

const ClockDescription& ClockProvider::get_clock_by_name(const std::string& name)
{
    for (const auto& clock : clocks_)
    {
        if (name == clock.name)
        {
            return clock;
        }
    }

    using namespace std::literals::string_literals;
    throw InvalidClock("clock \'"s + name + "\' is not available"s);
}
} // namespace lo2s::time
