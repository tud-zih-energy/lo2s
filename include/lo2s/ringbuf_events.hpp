/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

namespace lo2s
{
enum class EventType : uint64_t
{
    CCTX_ENTER = 1,
    CCTX_LEAVE = 2,
    CCTX_SAMPLE = 3,
    CCTX_DEF = 4,
    OMPT_ENTER =5,
    OMPT_EXIT = 6
};

struct __attribute__((packed)) event_header
{
    EventType type;
    uint64_t size;
};

struct __attribute__((packed)) cctx_def
{
    struct event_header header;
    uint64_t addr;
    char function[1];
};

struct __attribute__((packed)) cctx_enter
{
    struct event_header header;
    uint64_t tp;
    uint64_t addr;
};

struct __attribute__((packed)) cctx_leave
{
    struct event_header header;
    uint64_t tp;
    uint64_t addr;
};

}
