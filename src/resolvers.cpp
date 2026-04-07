// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/resolvers.hpp>

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
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
}

void Resolvers::emplace_mappings_for(Process p, const Mapping& m, const std::string& binary_name)
{
    auto fr = function_resolver_for(binary_name);
    if (fr != nullptr)
    {
        function_resolvers.at(p).emplace(m, fr);
    }
}
} // namespace lo2s
