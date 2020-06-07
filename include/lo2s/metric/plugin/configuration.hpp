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

#include <nitro/env/get.hpp>
#include <nitro/lang/string.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <cctype>

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
        auto events = read_env_variable(std::string("METRIC_") + upper_case(name));

        if (events.empty())
        {
            events = read_env_variable(std::string("METRIC_") + upper_case(name) + "_PLUGIN");
        }

        events_ = nitro::lang::split(events, ",");
    }

    static std::string read_env_variable(const std::string& name)
    {
        try
        {
            return nitro::env::get(std::string("LO2S_") + name, nitro::env::no_default);
        }
        catch (std::exception& e)
        {
            return nitro::env::get(std::string("SCOREP_") + name);
        }
    }

    static std::string upper_case(std::string input)
    {
        std::transform(input.begin(), input.end(), input.begin(), ::toupper);

        return input;
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
