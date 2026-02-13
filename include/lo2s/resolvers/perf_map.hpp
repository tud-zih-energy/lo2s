/*
 * this file is part of the lo2s software.
 * linux otf2 sampling
 *
 * copyright (c) 2016,
 *    technische universitaet dresden, germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with lo2s.  if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <lo2s/function_resolver.hpp>

#include <regex>

namespace lo2s
{
class PerfMap : public FunctionResolver
{
public:
    PerfMap(Process process);

    static std::shared_ptr<PerfMap> cache(Process process)
    {
        static std::map<Process, std::shared_ptr<PerfMap>> m;
        return m.emplace(process, std::make_shared<PerfMap>(process)).first->second;
    }

    Mapping mapping()
    {
        return { start_, end_, 0 };
    }

    LineInfo lookup_line_info(Address addr) override
    {
        auto it = entries_.find(addr + start_);
        if (it != entries_.end())
        {
            return it->second;
        }
        return LineInfo::for_unknown_function();
    }

private:
    std::map<Range, LineInfo> entries_;
    Address start_ = UINT64_MAX;
    Address end_ = UINT64_MAX;
};
} // namespace lo2s
