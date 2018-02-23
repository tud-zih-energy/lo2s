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

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "lo2s/perf/counter_description.hpp"

namespace lo2s
{
namespace perf
{

class EventProvider
{
public:
    struct DescriptionCache
    {
    private:
        DescriptionCache()
        : description(std::string(), static_cast<perf_type_id>(-1), 0, 0), valid_(false)
        {
        }

    public:
        DescriptionCache(const CounterDescription& description)
        : description(description), valid_(true)
        {
        }

        DescriptionCache(CounterDescription&& description)
        : description(std::move(description)), valid_(true)
        {
        }

        static DescriptionCache make_invalid()
        {
            return DescriptionCache();
        }

        bool is_valid() const
        {
            return valid_;
        }

        CounterDescription description;

    private:
        bool valid_;
    };

    using EventMap = std::unordered_map<std::string, DescriptionCache>;

    EventProvider();
    EventProvider(const EventProvider&) = delete;
    void operator=(const EventProvider&) = delete;

    static const EventProvider& instance()
    {
        return instance_mutable();
    }

    static const CounterDescription& get_event_by_name(const std::string& name);

    static bool has_event(const std::string& name);

    static std::vector<std::string> get_predefined_event_names();
    static std::vector<std::string> get_pmu_event_names();

    class InvalidEvent : public std::runtime_error
    {
    public:
        InvalidEvent(const std::string& event_description) : std::runtime_error(event_description)
        {
        }
    };

private:
    static EventProvider& instance_mutable()
    {
        static EventProvider e;
        return e;
    }

    const CounterDescription& cache_event(const std::string& name);

    EventMap event_map_;
};
}
}
