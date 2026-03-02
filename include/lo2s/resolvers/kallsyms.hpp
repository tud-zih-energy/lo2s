// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
