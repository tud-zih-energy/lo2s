// SPDX-FileCopyrightText: 2021 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <fmt/format.h>

namespace lo2s
{

enum class MeasurementType
{
    SAMPLE,
    GROUP_METRIC,
    USERSPACE_METRIC,
    BIO,
    SYSCALL,
    GPU,
    TRACEPOINT,
    POSIX_IO,
    OPENMP,
    UNKNOWN,
};

} // namespace lo2s
