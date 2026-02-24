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

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/line_info.hpp>

#include <map>
#include <string>

namespace lo2s
{
class ManualFunctionResolver : public FunctionResolver
{
public:
    ManualFunctionResolver(const std::string& name, std::map<Address, std::string>& functions)
    : FunctionResolver(name), functions_(functions)
    {
    }

    LineInfo lookup_line_info(Address address) override
    {
        if (functions_.count(address))
        {
            return LineInfo::for_function("", functions_.at(address).c_str(), 1, name_);
        }

        return LineInfo::for_unknown_function();
    }

protected:
    std::map<Address, std::string> functions_;
};

} // namespace lo2s
