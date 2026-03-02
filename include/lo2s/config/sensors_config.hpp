// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>

namespace lo2s
{
struct SensorsConfig
{

    static void add_parser(nitro::options::parser& parser);
    SensorsConfig(nitro::options::arguments& arguments);
    void check();
    bool enabled = false;
    std::chrono::milliseconds read_interval = std::chrono::milliseconds(0);
};

void to_json(nlohmann::json& j, const SensorsConfig& config);
} // namespace lo2s
