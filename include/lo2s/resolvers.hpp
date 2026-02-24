/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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

#include <lo2s/address.hpp>
#include <lo2s/instruction_resolver.hpp>
#include <lo2s/memory_map.hpp>
#include <lo2s/resolvers/manual_function_resolver.hpp>
#include <lo2s/types/process.hpp>

#include <map>
#include <string>

namespace lo2s
{

struct Resolvers
{
    std::map<Process, ProcessFunctionMap> function_resolvers;
    std::map<Process, MemoryMap<InstructionResolver>> instruction_resolvers;

    std::map<Process, MemoryMap<ManualFunctionResolver>> gpu_function_resolvers;

    void fork(Process parent, Process process);

    void emplace_mappings_for(Process p, const Mapping& m, const std::string& binary_name);
};
} // namespace lo2s
