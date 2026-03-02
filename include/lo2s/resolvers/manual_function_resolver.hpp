// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
