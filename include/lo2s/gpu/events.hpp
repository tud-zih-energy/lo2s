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

#include <lo2s/ringbuf_events.hpp>

namespace lo2s
{
namespace gpu
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

} // namespace gpu
} // namespace lo2s
