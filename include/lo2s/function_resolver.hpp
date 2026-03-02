// SPDX-FileCopyrightText: 2024 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/util.hpp>

#include <memory>
#include <string>
#include <utility>

namespace lo2s
{
class FunctionResolver
{
public:
    FunctionResolver(std::string name) : name_(std::move(name))
    {
    }

    FunctionResolver(FunctionResolver&) = delete;
    FunctionResolver(FunctionResolver&&) = delete;
    FunctionResolver& operator=(FunctionResolver&&) = delete;
    FunctionResolver& operator=(FunctionResolver&) = delete;

    static std::shared_ptr<FunctionResolver> cache(const std::string& name)
    {
        return BinaryCache<FunctionResolver>::instance()[name];
    }

    virtual LineInfo lookup_line_info(Address addr [[maybe_unused]])
    {
        return LineInfo::for_binary(name_);
    }

    std::string name()
    {
        return name_;
    }

    virtual ~FunctionResolver() = default;

protected:
    std::string name_;
};

std::shared_ptr<FunctionResolver> function_resolver_for(const std::string& filename);
} // namespace lo2s
