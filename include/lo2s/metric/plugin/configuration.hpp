/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2020,
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

#include <lo2s/util/env.hpp>
#include <lo2s/util/string.hpp>

#include <nitro/lang/string.hpp>

#include <string>
#include <vector>

namespace lo2s
{
namespace metric
{
namespace plugin
{

class Configuration
{
public:
    Configuration(const std::string& name) : name_(name)
    {
        auto events = util::get_scorep_compatible_env_variable(std::string("METRIC_") +
                                                               util::transform_upper_case(name));

        if (events.empty())
        {
            events = util::get_scorep_compatible_env_variable(
                std::string("METRIC_") + util::transform_upper_case(name) + "_PLUGIN");
        }

        events_ = nitro::lang::split(events, ",");
    }

    const std::string& name() const
    {
        return name_;
    }

    const std::vector<std::string>& events() const
    {
        return events_;
    }

private:
    std::string name_;
    std::vector<std::string> events_;
};

} // namespace plugin
} // namespace metric
} // namespace lo2s
