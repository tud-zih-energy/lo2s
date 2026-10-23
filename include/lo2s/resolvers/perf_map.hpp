// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/log.hpp>
#include <lo2s/types/process.hpp>

#include <map>
#include <memory>

#include <cstdint>

namespace lo2s
{
class PerfMap : public FunctionResolver
{
public:
    explicit PerfMap(Process process);

    static std::shared_ptr<PerfMap> cache(Process process)
    {
        static std::map<Process, std::shared_ptr<PerfMap>> m;
        return m.emplace(process, std::make_shared<PerfMap>(process)).first->second;
    }

    Mapping mapping()
    {
        return { start_, end_, Address{ 0 } };
    }

    LineInfo lookup_line_info(Address addr, uint64_t offset [[maybe_unused]] = 0) override
    {
        const auto it = entries_.find(Range{ addr });
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
    Address start_ = Address::max();
    Address end_ = Address::max();
};
} // namespace lo2s
