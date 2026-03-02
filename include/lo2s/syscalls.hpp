// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace lo2s
{

std::string syscall_name_for_nr(int syscall_nr);

int syscall_nr_for_name(const std::string& name);

} // namespace lo2s
