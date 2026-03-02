// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace lo2s
{
class Core
{
public:
    Core(int64_t core_id, int64_t package_id) : core_id_(core_id), package_id_(package_id)
    {
    }

    static Core invalid()
    {
        return { -1, -1 };
    }

    friend bool operator==(const Core& lhs, const Core& rhs)
    {
        return (lhs.core_id_ == rhs.core_id_) && (lhs.package_id_ == rhs.package_id_);
    }

    friend bool operator<(const Core& lhs, const Core& rhs)
    {
        if (lhs.package_id_ == rhs.package_id_)
        {
            return lhs.core_id_ < rhs.core_id_;
        }
        return lhs.package_id_ < rhs.package_id_;
    }

    int64_t core_as_int() const
    {
        return core_id_;
    }

    int64_t package_as_int() const
    {
        return package_id_;
    }

private:
    int64_t core_id_;
    int64_t package_id_;
};
} // namespace lo2s
