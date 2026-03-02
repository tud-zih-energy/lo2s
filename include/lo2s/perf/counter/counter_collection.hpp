// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/perf/event_attr.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

namespace lo2s::perf::counter
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
            if (!leader)
            {
                throw std::runtime_error("Trying to get scale for non-existent leader!");
            }
            return leader.value().scale();
        }
        return counters[index - 1].scale();
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

} // namespace lo2s::perf::counter
