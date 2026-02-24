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
#include <memory>

#include <cstdint>

namespace lo2s
{
class Kallsyms : public FunctionResolver
{
public:
    Kallsyms();

    static std::shared_ptr<Kallsyms> cache()
    {
        const static std::shared_ptr<Kallsyms> k = std::make_shared<Kallsyms>();
        return k;
    }

    uint64_t start() const
    {
        return start_;
    }

    LineInfo lookup_line_info(Address addr) override
    {
        auto it = kallsyms_.find(addr + start_);
        if (it != kallsyms_.end())
        {
            return LineInfo::for_function("", it->second.c_str(), 1, "[kernel]");
        }
        return LineInfo::for_binary("[kernel]");
    }

private:
    std::map<Range, std::string> kallsyms_;
    uint64_t start_ = UINT64_MAX;
};
} // namespace lo2s
