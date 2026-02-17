/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2026,
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

#include <atomic>

#include <cstdint>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{

// To resolve possible ringbuf format incompatibilities
// Increase everytime you:
//  - change the ringbuf_header
//  - add, delete or change events
constexpr uint64_t RINGBUF_VERSION = 2;

enum class RingbufMeasurementType : uint64_t
{
    GPU = 0,
    OPENMP = 1,
};

struct ringbuf_header
{
    uint64_t version;
    uint64_t size;
    std::atomic_uint64_t head;
    std::atomic_uint64_t tail;
    // set by the writer side (CUPTI, etc.)
    int64_t pid;

    // set by the reader size (lo2s)
    std::atomic<uint64_t> lo2s_ready;
    clockid_t clockid;
};

} // namespace lo2s
