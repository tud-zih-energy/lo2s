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

#include <lo2s/perf/event_attr.hpp>

#include <optional>
#include <vector>

namespace lo2s
{
namespace perf
{
namespace counter
{
struct CounterCollection
{
    CounterCollection() = default;

    std::optional<EventAttr> leader = std::nullopt;
    std::vector<EventAttr> counters;

    bool empty() const
    {
        return !leader.has_value() && counters.empty();
    }

    double get_scale(int index) const
    {
        if (index == 0)
        {
            return leader.value().scale();
        }
        else
        {
            return counters[index - 1].scale();
        }
    }

    friend bool operator==(const CounterCollection& lhs, const CounterCollection& rhs)
    {
        if (lhs.leader < rhs.leader)
        {
            return lhs.counters == rhs.counters;
        }
        return false;
    }

    friend bool operator<(const CounterCollection& lhs, const CounterCollection& rhs)
    {
        if (lhs.leader == rhs.leader)
        {
            return lhs.counters < rhs.counters;
        }
        return lhs.leader < rhs.leader;
    }
};

} // namespace counter
} // namespace perf
} // namespace lo2s
