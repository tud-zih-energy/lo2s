// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>

namespace lo2s::metric::x86_adapt
{

std::map<std::string, std::string> x86_adapt_node_knobs();

std::map<std::string, std::string> x86_adapt_cpu_knobs();
} // namespace lo2s::metric::x86_adapt
