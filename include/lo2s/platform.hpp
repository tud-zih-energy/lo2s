/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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
/*
 * mem_event.h
 *
 *  Created on: 20.02.2015
 *      Author: rschoene
 */

#pragma once

#include <string>
#include <vector>

#include <cstdint>

#include <lo2s/perf/reader.hpp>

/* gracefully copied from https://github.com/deater/perf_event_tests/blob/master/ */

namespace lo2s
{
namespace platform
{

enum class Vendor
{
    UNKNOWN = -1,
    INTEL = 1,
    AMD = 2,
    IBM = 3,
    ARM = 4,
};

enum class Processor
{
    UNKNOWN = -1,
    PENTIUM_PRO = 1,
    PENTIUM_II = 2,
    PENTIUM_III = 3,
    PENTIUM_4 = 4,
    PENTIUM_M = 5,
    COREDUO = 6,
    CORE2 = 7,
    NEHALEM = 8,
    NEHALEM_EX = 9,
    WESTMERE = 10,
    WESTMERE_EX = 11,
    SANDYBRIDGE = 12,
    ATOM = 13,
    K7 = 14,
    K8 = 15,
    AMD_FAM10H = 16,
    AMD_FAM11H = 17,
    AMD_FAM14H = 18,
    AMD_FAM15H = 19,
    IVYBRIDGE = 20,
    KNIGHTSCORNER = 21,
    SANDYBRIDGE_EP = 22,
    AMD_FAM16H = 23,
    IVYBRIDGE_EP = 24,
    HASWELL = 25,
    ATOM_CEDARVIEW = 26,
    ATOM_SILVERMONT = 27,
    BROADWELL = 28,
    HASWELL_EP = 29,
    POWER3 = 103,
    POWER4 = 104,
    POWER5 = 105,
    POWER6 = 106,
    POWER7 = 107,
    CORTEX_A8 = 200,
    CORTEX_A9 = 201,
    CORTEX_A5 = 202,
    CORTEX_A15 = 203,
    ARM1176 = 204,
};

std::vector<perf::SysfsEvent> get_mem_events();
} // namespace platform
} // namespace lo2s
