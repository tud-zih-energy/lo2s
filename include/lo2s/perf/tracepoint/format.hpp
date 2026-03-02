// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <utility>

#include <cstddef>

namespace lo2s::perf::tracepoint
{
class EventField
{
public:
    EventField() = default;

    EventField(std::string name, std::ptrdiff_t offset, std::size_t size)
    : name_(std::move(name)), offset_(offset), size_(size)
    {
    }

    const std::string& name() const
    {
        return name_;
    }

    std::ptrdiff_t offset() const
    {
        return offset_;
    }

    std::size_t size() const
    {
        return size_;
    }

    bool is_integer() const
    {
        // Parsing the type name is hard... really you don't want to do that
        switch (size())
        {
        case 1:
        case 2:
        case 4:
        case 8:
            return true;
        default:
            return false;
        }
    }

    bool valid() const
    {
        return size_ > 0;
    }

private:
    std::string name_;
    std::ptrdiff_t offset_;
    std::size_t size_ = 0;
};
} // namespace lo2s::perf::tracepoint
