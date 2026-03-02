// SPDX-FileCopyrightText: 2024 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/function_resolver.hpp>

#include <lo2s/dwarf_resolve.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

#include <exception>
#include <memory>
#include <string>

namespace lo2s
{

std::shared_ptr<FunctionResolver> function_resolver_for(const std::string& filename)
{
    std::shared_ptr<FunctionResolver> fr;
    if (known_non_executable(filename))
    {
        return nullptr;
    }

    try
    {
        fr = DwarfFunctionResolver::cache(filename);
    }
    catch (std::exception& e)
    {
        Log::trace() << "Could not open DWARF resolver for (" << filename << ") " << e.what();
        fr = FunctionResolver::cache(filename);
    }

    return fr;
}
} // namespace lo2s
