// SPDX-FileCopyrightText: 2024 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace lo2s
{

struct __attribute__((packed)) event_header
{
    uint64_t size;
    uint64_t type;
};

} // namespace lo2s
