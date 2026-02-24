/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2026,
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

#include <lo2s/resolvers.hpp>

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/instruction_resolver.hpp>
#include <lo2s/types/process.hpp>

#include <string>

namespace lo2s
{
void Resolvers::fork(Process parent, Process process)
{
    if (function_resolvers.count(parent) != 0)
    {
        function_resolvers.emplace(process, function_resolvers.at(parent));
    }
    instruction_resolvers[process] = instruction_resolvers[parent];
}

void Resolvers::emplace_mappings_for(Process p, const Mapping& m, const std::string& binary_name)
{
    auto fr = function_resolver_for(binary_name);
    if (fr != nullptr)
    {
        function_resolvers.at(p).emplace(m, fr);
    }
    auto ir = instruction_resolver_for(binary_name);

    if (ir != nullptr)
    {
        instruction_resolvers[p].emplace(m, ir);
    }
}
} // namespace lo2s
