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
 * mem_event.c
 *
 *  Created on: 20.02.2015
 *      Author: rschoene
 */

/* gracefully copied from https://github.com/deater/perf_event_tests/blob/master/ */

#include "platform.hpp"

#include <string>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace lo2s
{
namespace platform
{
    processor detect_processor(void)
    {
        FILE* fff;
        auto ven = vendor::UNKNOWN;
        int cpu_family = 0, model = 0;
        char string[BUFSIZ];
        fff = fopen("/proc/cpuinfo", "r");
        if (fff == NULL)
        {
            fprintf(stderr, "ERROR! Can't open /proc/cpuinfo\n");
            return processor::UNKNOWN;
        }
        while (1)
        {
            if (fgets(string, BUFSIZ, fff) == NULL)
                break;
            /* Power6 */
            if (strstr(string, "POWER6"))
            {
                ven = vendor::IBM;
                return processor::POWER6;
            }
            /* ARM */
            if (strstr(string, "CPU part"))
            {
                ven = vendor::ARM;
                if (strstr(string, "0xc05"))
                {
                    return processor::CORTEX_A5;
                }
                if (strstr(string, "0xc09"))
                {
                    return processor::CORTEX_A9;
                }
                if (strstr(string, "0xc08"))
                {
                    return processor::CORTEX_A8;
                }
                if (strstr(string, "0xc0f"))
                {
                    return processor::CORTEX_A15;
                }
                if (strstr(string, "0xb76"))
                {
                    return processor::ARM1176;
                }
                // Cortex R4 - 0xc14
                // Cortex R5 - 0xc15
                // ARM1136 - 0xb36
                // ARM1156 - 0xb56
                // ARM1176 - 0xb76
                // ARM11 MPCore - 0xb02
            }
            /* vendor */
            if (strstr(string, "vendor_id"))
            {
                if (strstr(string, "GenuineIntel"))
                {
                    ven = vendor::INTEL;
                }
                if (strstr(string, "AuthenticAMD"))
                {
                    ven = vendor::AMD;
                }
            }
            /* family */
            if (strstr(string, "cpu family"))
            {
                sscanf(string, "%*s %*s %*s %d", &cpu_family);
            }
            /* model */
            if ((strstr(string, "model")) && (!strstr(string, "model name")))
            {
                sscanf(string, "%*s %*s %d", &model);
            }
        }
        fclose(fff);
        if (ven == vendor::AMD)
        {
            if (cpu_family == 0x6)
            {
                return processor::K7;
            }
            if (cpu_family == 0xf)
            {
                return processor::K8;
            }
            if (cpu_family == 0x10)
            {
                return processor::AMD_FAM10H;
            }
            if (cpu_family == 0x11)
            {
                return processor::AMD_FAM11H;
            }
            if (cpu_family == 0x14)
            {
                return processor::AMD_FAM14H;
            }
            if (cpu_family == 0x15)
            {
                return processor::AMD_FAM15H;
            }
            if (cpu_family == 0x16)
            {
                return processor::AMD_FAM16H;
            }
        }
        if (ven == vendor::INTEL)
        {
            if (cpu_family == 6)
            {
                switch (model)
                {
                case 1:
                    return processor::PENTIUM_PRO;
                case 3:
                case 5:
                case 6:
                    return processor::PENTIUM_II;
                case 7:
                case 8:
                case 10:
                case 11:
                    return processor::PENTIUM_III;
                case 9:
                case 13:
                    return processor::PENTIUM_M;
                case 14:
                    return processor::COREDUO;
                case 15:
                case 22:
                case 23:
                case 29:
                    return processor::CORE2;
                case 28:
                case 38:
                case 39:
                case 53:
                    return processor::ATOM;
                case 54:
                    return processor::ATOM_CEDARVIEW;
                case 55:
                case 77:
                    return processor::ATOM_SILVERMONT;
                case 26:
                case 30:
                case 31:
                    return processor::NEHALEM;
                case 46:
                    return processor::NEHALEM_EX;
                case 37:
                case 44:
                    return processor::WESTMERE;
                case 47:
                    return processor::WESTMERE_EX;
                case 42:
                    return processor::SANDYBRIDGE;
                case 45:
                    return processor::SANDYBRIDGE_EP;
                case 58:
                    return processor::IVYBRIDGE;
                case 62:
                    return processor::IVYBRIDGE_EP;
                case 60:
                case 69:
                case 70:
                    return processor::HASWELL;
                case 63:
                    return processor::HASWELL_EP;
                case 61:
                case 71:
                case 79:
                    return processor::BROADWELL;
                }
            }
            if (cpu_family == 11)
            {
                return processor::KNIGHTSCORNER;
            }
            if (cpu_family == 15)
            {
                return processor::PENTIUM_4;
            }
        }
        return processor::UNKNOWN;
    }

    std::vector<counter_description> get_mem_events()
    {
        auto proc = detect_processor();
        switch (proc)
        {
        case processor::HASWELL_EP:
            return { { "L1-read", PERF_TYPE_HW_CACHE, PERF_COUNT_HW_CACHE_L1D |
                                                          PERF_COUNT_HW_CACHE_OP_READ << 8 |
                                                          PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16 },
                     { "L1-write", PERF_TYPE_HW_CACHE,
                       PERF_COUNT_HW_CACHE_L1D | PERF_COUNT_HW_CACHE_OP_WRITE << 8 |
                           PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16 },
                     { "L2-read", PERF_TYPE_RAW,
                       0x3f0 }, /*(L2_TRANS:DEMAND_DATA_RD_HIT+L2_TRANS:RFO)*/
                     { "L2-write", PERF_TYPE_RAW, 0x10f0 }, /*(L2_TRANS:L1D_WB)*/
                     { "L3-read", PERF_TYPE_RAW, 0x5301b7, 0x3fb84101b3 },
                     { "L3-write", PERF_TYPE_RAW, 0x40f0 }, /*(L2_TRANS:L2_WB)*/
                     { "RAM", PERF_TYPE_RAW, 0x5301bb, 0x3fb84001b3 } };
        default:
            return { { "L2", PERF_TYPE_RAW, 0xff24 },
                     { "L3", PERF_TYPE_RAW, 0x4f26 },
                     { "RAM", PERF_TYPE_RAW, 0x412e } };
        }
    }
}
}
