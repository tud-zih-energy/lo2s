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

#include <lo2s/perf/event_description.hpp>

#include <vector>

namespace lo2s
{
namespace perf
{
namespace counter
{
struct CounterCollection
{
    EventDescription leader;
    std::vector<EventDescription> counters;

    double get_scale(int index) const
    {
        if (index == 0)
        {
            return leader.scale;
        }
        else
        {
            return counters[index - 1].scale;
        }
    }
};

const CounterCollection& requested_userspace_counters();
const CounterCollection& requested_group_counters();

} // namespace counter
} // namespace perf
} // namespace lo2s
