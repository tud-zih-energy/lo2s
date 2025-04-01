/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
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

#include <lo2s/dwarf_resolve.hpp>
#include <lo2s/function_resolver.hpp>

#include <nitro/lang/string.hpp>

namespace lo2s
{

std::shared_ptr<FunctionResolver> function_resolver_for(const std::string& filename)
{
    std::shared_ptr<FunctionResolver> fr;
    if (known_non_executable(filename))
    {
        return nullptr;
    }
    else
    {
        try
        {
            fr = DwarfFunctionResolver::cache(filename);
        }
        catch (std::exception& e)
        {
            Log::trace() << "Could not open DWARF resolver for (" << filename << ") " << e.what();
            fr = FunctionResolver::cache(filename);
        }
    }

    return fr;
}
} // namespace lo2s
