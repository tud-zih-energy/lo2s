// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
