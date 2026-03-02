// Copyright (C) 2024 ,
// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/rb/events.hpp>

#include <cstdint>

namespace lo2s::gpu
{

enum class EventType : uint64_t
{
    KERNEL = 0,
    KERNEL_DEF = 1,

};

struct __attribute__((packed)) kernel_def
{
    struct event_header header;
    uint64_t kernel_id;
    char function[1];
};

struct __attribute__((packed)) kernel
{
    struct event_header header;
    uint64_t start_tp;
    uint64_t end_tp;
    uint64_t kernel_id;
};

} // namespace lo2s::gpu
