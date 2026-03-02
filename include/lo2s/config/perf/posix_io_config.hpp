// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>

namespace lo2s::perf
{

struct PosixIOConfig
{
    PosixIOConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);

    std::chrono::nanoseconds read_interval = std::chrono::nanoseconds(0);
    bool enabled = false;
};

void to_json(nlohmann::json& j, const PosixIOConfig& config);
} // namespace lo2s::perf
