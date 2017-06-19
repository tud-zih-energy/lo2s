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
#include <unordered_map>

#include "lo2s/platform.hpp"

namespace lo2s
{
namespace perf
{

class EventProvider
{
public:
    using EventMap = std::unordered_map<std::string, platform::CounterDescription>;

    EventProvider();
    EventProvider(const EventProvider&) = delete;
    void operator=(const EventProvider&) = delete;

    static const EventProvider& instance()
    {
        static EventProvider e;
        return e;
    }

    static inline const EventMap::mapped_type& get_event_by_name(const std::string& name)
    {
        return instance().event_map_.at(name);
    }

    static inline bool has_event(const std::string& name)
    {
        return instance().event_map_.count(name);
    }

    static inline const EventMap& event_map()
    {
        return instance().event_map_;
    }

private:
    EventMap event_map_;
};
}
}
