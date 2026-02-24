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

#include <lo2s/metric/x86_adapt/knobs.hpp>

#include <lo2s/log.hpp>

#include <x86_adapt_cxx/exception.hpp>
#include <x86_adapt_cxx/x86_adapt.hpp>

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <utility>

namespace lo2s::metric::x86_adapt
{

std::map<std::string, std::string> x86_adapt_node_knobs()
{
    std::map<std::string, std::string> knobs;
    try
    {
        ::x86_adapt::x86_adapt x86_adapt;
        auto node_cfg_items = x86_adapt.node_configuration_items();

        for (const auto& item : node_cfg_items)
        {
            knobs.emplace(std::piecewise_construct, std::forward_as_tuple(item.name()),
                          std::forward_as_tuple(item.description()));
        }
    }
    catch (const ::x86_adapt::x86_adapt_error& e)
    {
        Log::debug() << "Failed to access x86_adapt node knobs! (error: " << e.what() << ")";
    }
    return knobs;
}

std::map<std::string, std::string> x86_adapt_cpu_knobs()
{
    std::map<std::string, std::string> node_knobs = x86_adapt_node_knobs();
    std::map<std::string, std::string> cpu_knobs;
    ::x86_adapt::x86_adapt x86_adapt;

    auto cpu_cfg_items = x86_adapt.cpu_configuration_items();
    for (const auto& item : cpu_cfg_items)
    {
        cpu_knobs.emplace(std::piecewise_construct, std::forward_as_tuple(item.name()),
                          std::forward_as_tuple(item.description()));
    }
    std::set_difference(cpu_knobs.begin(), cpu_knobs.end(), node_knobs.begin(), node_knobs.end(),
                        std::inserter(cpu_knobs, cpu_knobs.end()), cpu_knobs.value_comp());
    return cpu_knobs;
}
} // namespace lo2s::metric::x86_adapt
