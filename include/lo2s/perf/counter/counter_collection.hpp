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

#include <lo2s/perf/event.hpp>

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
    std::optional<Event> leader_;
    std::vector<Event> counters;

    double get_scale(int index) const
    {
        if (index == 0)
        {
            return leader_.value().scale();
        }
        else
        {
            return counters[index - 1].scale();
        }
    }

    Event& leader() const
    {
        return const_cast<Event&>(leader_.value());
    }

    friend bool operator==(const CounterCollection& lhs, const CounterCollection& rhs)
    {
        if (lhs.leader_.value() == rhs.leader_.value())
        {
            return lhs.counters == rhs.counters;
        }
        return false;
    }

    friend bool operator<(const CounterCollection& lhs, const CounterCollection& rhs)
    {
        if (lhs.leader_.value() == rhs.leader_.value())
        {
            return lhs.counters < rhs.counters;
        }
        return lhs.leader_.value() < rhs.leader_.value();
    }
};

} // namespace counter
} // namespace perf
} // namespace lo2s
