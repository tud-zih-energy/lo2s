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

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <string>
#include <vector>

extern "C" {
#include <linux/perf_event.h>
}

/* gracefully copied from https://github.com/deater/perf_event_tests/blob/master/ */

namespace lo2s
{
namespace platform
{
#ifdef __powerpc__
#define rmb() asm volatile("sync" : : : "memory")
#elif defined(__s390__)
#define rmb() asm volatile("bcr 15,0" ::: "memory")
#elif defined(__sh__)
#if defined(__SH4A__) || defined(__SH5__)
#define rmb() asm volatile("synco" ::: "memory")
#else
#define rmb() asm volatile("" ::: "memory")
#endif
#elif defined(__hppa__)
#define rmb() asm volatile("" ::: "memory")
#elif defined(__sparc__)
#define rmb() asm volatile("" ::: "memory")
#elif defined(__alpha__)
#define rmb() asm volatile("mb" ::: "memory")
#elif defined(__ia64__)
#define rmb() asm volatile("mf" ::: "memory")
#elif defined(__arm__)
/*
 * Use the __kuser_memory_barrier helper in the CPU helper page. See
 * arch/arm/kernel/entry-armv.S in the kernel source for details.
 */
#define rmb() ((void (*)(void))0xffff0fa0)()
#elif defined(__aarch64__)
#define rmb() asm volatile("dmb ld" ::: "memory")
#elif defined(__mips__)
#define rmb()                                                                                      \
    asm volatile(".set mips2\n\t"                                                                  \
                 "sync\n\t"                                                                        \
                 ".set mips0"                                                                      \
                 : /* no output */                                                                 \
                 : /* no input */                                                                  \
                 : "memory")
#elif defined(__i386__)
#define rmb() asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#elif defined(__x86_64)
#if defined(__KNC__)
#define rmb() __sync_synchronize()
#else
#define rmb() asm volatile("lfence" ::: "memory")
#endif
#else
#error Need to define rmb for this architecture!
#error See the kernel source directory: tools/perf/perf.h file
#endif

    enum class vendor
    {
        UNKNOWN = -1,
        INTEL = 1,
        AMD = 2,
        IBM = 3,
        ARM = 4,
    };

    enum class processor
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

    struct counter_description
    {
        counter_description(const std::string& name, perf_type_id type, uint64_t config,
                            uint64_t config1 = 0)
        : name(name), type(type), config(config), config1(config1)
        {
        }

        std::string name;
        perf_type_id type;
        uint64_t config;
        uint64_t config1;
    };

    std::vector<counter_description> get_mem_events();
}
}
#endif /* PLATFORM_H_ */
