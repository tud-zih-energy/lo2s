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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mem_levels.h"

#include "mem_event.h"

int detect_processor(void)
{
    FILE* fff;
    int vendor = VENDOR_UNKNOWN, cpu_family = 0, model = 0;
    char string[BUFSIZ];
    fff = fopen("/proc/cpuinfo", "r");
    if (fff == NULL)
    {
        fprintf(stderr, "ERROR! Can't open /proc/cpuinfo\n");
        return PROCESSOR_UNKNOWN;
    }
    while (1)
    {
        if (fgets(string, BUFSIZ, fff) == NULL)
            break;
        /* Power6 */
        if (strstr(string, "POWER6"))
        {
            vendor = VENDOR_IBM;
            return PROCESSOR_POWER6;
        }
        /* ARM */
        if (strstr(string, "CPU part"))
        {
            vendor = VENDOR_ARM;
            if (strstr(string, "0xc05"))
            {
                return PROCESSOR_CORTEX_A5;
            }
            if (strstr(string, "0xc09"))
            {
                return PROCESSOR_CORTEX_A9;
            }
            if (strstr(string, "0xc08"))
            {
                return PROCESSOR_CORTEX_A8;
            }
            if (strstr(string, "0xc0f"))
            {
                return PROCESSOR_CORTEX_A15;
            }
            if (strstr(string, "0xb76"))
            {
                return PROCESSOR_ARM1176;
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
                vendor = VENDOR_INTEL;
            }
            if (strstr(string, "AuthenticAMD"))
            {
                vendor = VENDOR_AMD;
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
    if (vendor == VENDOR_AMD)
    {
        if (cpu_family == 0x6)
        {
            return PROCESSOR_K7;
        }
        if (cpu_family == 0xf)
        {
            return PROCESSOR_K8;
        }
        if (cpu_family == 0x10)
        {
            return PROCESSOR_AMD_FAM10H;
        }
        if (cpu_family == 0x11)
        {
            return PROCESSOR_AMD_FAM11H;
        }
        if (cpu_family == 0x14)
        {
            return PROCESSOR_AMD_FAM14H;
        }
        if (cpu_family == 0x15)
        {
            return PROCESSOR_AMD_FAM15H;
        }
        if (cpu_family == 0x16)
        {
            return PROCESSOR_AMD_FAM16H;
        }
    }
    if (vendor == VENDOR_INTEL)
    {
        if (cpu_family == 6)
        {
            switch (model)
            {
            case 1:
                return PROCESSOR_PENTIUM_PRO;
            case 3:
            case 5:
            case 6:
                return PROCESSOR_PENTIUM_II;
            case 7:
            case 8:
            case 10:
            case 11:
                return PROCESSOR_PENTIUM_III;
            case 9:
            case 13:
                return PROCESSOR_PENTIUM_M;
            case 14:
                return PROCESSOR_COREDUO;
            case 15:
            case 22:
            case 23:
            case 29:
                return PROCESSOR_CORE2;
            case 28:
            case 38:
            case 39:
            case 53:
                return PROCESSOR_ATOM;
            case 54:
                return PROCESSOR_ATOM_CEDARVIEW;
            case 55:
            case 77:
                return PROCESSOR_ATOM_SILVERMONT;
            case 26:
            case 30:
            case 31:
                return PROCESSOR_NEHALEM;
            case 46:
                return PROCESSOR_NEHALEM_EX;
            case 37:
            case 44:
                return PROCESSOR_WESTMERE;
            case 47:
                return PROCESSOR_WESTMERE_EX;
            case 42:
                return PROCESSOR_SANDYBRIDGE;
            case 45:
                return PROCESSOR_SANDYBRIDGE_EP;
            case 58:
                return PROCESSOR_IVYBRIDGE;
            case 62:
                return PROCESSOR_IVYBRIDGE_EP;
            case 60:
            case 69:
            case 70:
                return PROCESSOR_HASWELL;
            case 63:
                return PROCESSOR_HASWELL_EP;
            case 61:
            case 71:
            case 79:
                return PROCESSOR_BROADWELL;
            }
        }
        if (cpu_family == 11)
        {
            return PROCESSOR_KNIGHTSCORNER;
        }
        if (cpu_family == 15)
        {
            return PROCESSOR_PENTIUM_4;
        }
    }
    return PROCESSOR_UNKNOWN;
}

int get_latency_load_event(unsigned long long* config, unsigned long long* config1, int* precise_ip)
{
    int processor, processor_notfound = 0;
    processor = detect_processor();
    switch (processor)
    {
    case PROCESSOR_SANDYBRIDGE:
    case PROCESSOR_SANDYBRIDGE_EP:
        *config = 0x1cd;
        *config1 = 0x3;
        *precise_ip = 2;
        break;
    case PROCESSOR_IVYBRIDGE:
    case PROCESSOR_IVYBRIDGE_EP:
        *config = 0x1cd;
        *config1 = 0x3;
        *precise_ip = 2;
        break;
    case PROCESSOR_HASWELL:
    case PROCESSOR_HASWELL_EP:
        *config = 0x1cd;
        *config1 = 0x3;
        *precise_ip = 2;
        break;
    case PROCESSOR_BROADWELL:
        *config = 0x1cd;
        *config1 = 0x3;
        *precise_ip = 2;
        break;
    default:
        *config = 0x0;
        *config1 = 0x0;
        *precise_ip = 0;
        processor_notfound = -1;
    }
    return processor_notfound;
}
int get_latency_store_event(unsigned long long* config, unsigned long long* config1,
                            int* precise_ip)
{
    int processor, processor_notfound = 0;
    processor = detect_processor();
    switch (processor)
    {
    case PROCESSOR_SANDYBRIDGE:
    case PROCESSOR_SANDYBRIDGE_EP:
        *config = 0x2cd;
        *config1 = 0x0;
        *precise_ip = 2;
        break;
    case PROCESSOR_IVYBRIDGE:
    case PROCESSOR_IVYBRIDGE_EP:
        *config = 0x2cd;
        *config1 = 0x0;
        *precise_ip = 2;
        break;
    case PROCESSOR_HASWELL:
    case PROCESSOR_HASWELL_EP:
        *config = 0x2cd;
        *config1 = 0x0;
        *precise_ip = 2;
        break;
    case PROCESSOR_BROADWELL:
        *config = 0x2cd;
        *config1 = 0x0;
        *precise_ip = 2;
        break;
    default:
        *config = 0x0;
        *config1 = 0x0;
        *precise_ip = 0;
        processor_notfound = -1;
    }
    return processor_notfound;
}

uint64_t get_mem_event(int mem_level)
{
    switch (mem_level)
    {
        case MEM_LEVEL_L1:
            return -1;
        case MEM_LEVEL_L2:
            return 0xff24;
        case MEM_LEVEL_L3:
            return 0x4f2e;
        case MEM_LEVEL_RAM:
            return 0x412e;
        default:
            return -1;
    }
}
