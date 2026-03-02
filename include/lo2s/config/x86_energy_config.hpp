// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

namespace lo2s
{
struct X86EnergyConfig
{
    X86EnergyConfig(nitro::options::arguments& arguments);
    static void add_parser(nitro::options::parser& parser);
    void check() const;

    bool enabled = false;
};

void to_json(nlohmann::json& j, const X86EnergyConfig& config);
} // namespace lo2s
