// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/types/process.hpp>

#include <map>
#include <memory>

#include <cstdint>

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

    bool has_perf_map()
    {
        return !entries_.empty();
    }

private:
    std::map<Range, LineInfo> entries_;
    Address start_ = UINT64_MAX;
    Address end_ = UINT64_MAX;
};
} // namespace lo2s
