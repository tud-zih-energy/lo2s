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
#ifndef X86_RECORD_OTF2_MEM_LEVELS_H
#define X86_RECORD_OTF2_MEM_LEVELS_H

enum mem_levels
{
    MEM_LEVEL_L1 = 0,
    MEM_LEVEL_L2 = 1,
    MEM_LEVEL_L3 = 2,
    MEM_LEVEL_RAM = 3,
    MEM_LEVEL_MAX, /* non-ABI */
};

#endif // X86_RECORD_OTF2_MEM_LEVELS_HPP
