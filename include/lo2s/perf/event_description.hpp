/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <string>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{
namespace perf
{
enum class Availability
{
    UNAVAILABLE,
    SYSTEM_MODE,
    PROCESS_MODE,
    UNIVERSAL
};

struct EventDescription
{
    EventDescription(const std::string& name, perf_type_id type, std::uint64_t config,
                     std::uint64_t config1 = 0, double scale = 1, std::string unit = "#")
    : name(name), type(type), config(config), config1(config1), scale(scale), unit(unit),
      availability(Availability::UNAVAILABLE)
    {
    }

    EventDescription()
    : name(""), type(static_cast<perf_type_id>(-1)), config(0), config1(0), scale(1), unit("#"),
      availability(Availability::UNAVAILABLE)
    {
    }

    std::string name;
    perf_type_id type;
    std::uint64_t config;
    std::uint64_t config1;
    double scale;
    std::string unit;
    Availability availability;
};
} // namespace perf
} // namespace lo2s
