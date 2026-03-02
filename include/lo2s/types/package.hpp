// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace lo2s
{
class Package
{
public:
    explicit Package(int64_t id) : id_(id)
    {
    }

    static Package invalid()
    {
        return Package(-1);
    }

    friend bool operator==(const Package& lhs, const Package& rhs)
    {
        return lhs.id_ == rhs.id_;
    }

    friend bool operator<(const Package& lhs, const Package& rhs)
    {
        return lhs.id_ < rhs.id_;
    }

    int64_t as_int() const
    {
        return id_;
    }

private:
    int64_t id_;
};
} // namespace lo2s
