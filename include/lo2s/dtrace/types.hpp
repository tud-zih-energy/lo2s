/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

#include <nitro/lang/string_ref.hpp>

extern "C"
{
    struct dtrace_hdl;
    struct dtrace_probedata;
    struct dtrace_recdesc;
    struct dtrace_bufdata;
    struct dtrace_proginfo;
}

namespace lo2s
{
namespace dtrace
{
using Handle = struct dtrace_hdl*;
using ProbeDataPtr = struct dtrace_probedata*;
using RecDescPtr = struct dtrace_recdesc*;
using BufDataPtr = struct dtrace_bufdata*;

using ConstProbeDataPtr = const struct dtrace_probedata*;
using ConstRecDescPtr = const struct dtrace_recdesc*;
using ConstBufDataPtr = const struct dtrace_bufdata*;

struct ProbeDesc
{
    nitro::lang::string_ref provider;
    nitro::lang::string_ref module;
    nitro::lang::string_ref function;
    nitro::lang::string_ref name;
};

} // namespace dtrace
} // namespace lo2s
