// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
