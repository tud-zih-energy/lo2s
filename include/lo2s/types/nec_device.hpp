// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include <fmt/base.h>
#include <fmt/format.h>

namespace lo2s
{
class NecDevice
{
public:
    explicit NecDevice(int64_t id) : id_(id)
    {
    }

    int64_t as_int() const
    {
        return id_;
    }

    friend bool operator==(const NecDevice& lhs, const NecDevice& rhs)
    {
        return lhs.id_ == rhs.id_;
    }

    friend bool operator<(const NecDevice& lhs, const NecDevice& rhs)
    {
        return lhs.id_ < rhs.id_;
    }

private:
    int64_t id_;
};
} // namespace lo2s

namespace fmt
{
template <>
struct formatter<lo2s::NecDevice>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        const auto* it = ctx.begin();
        const auto* end = ctx.end();
        if (it != end && *it != '}')
        {
            throw format_error("invalid format");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const lo2s::NecDevice& device, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "VE {}", device.as_int());
    }
};
} // namespace fmt
