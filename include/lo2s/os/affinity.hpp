/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016-2018,
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

namespace lo2s
{
namespace os
{
class affinity
{
public:
    static void set(std::size_t cpu_id)
    {
        cpu_set_t mask;

        CPU_ZERO(&mask);

        CPU_SET(cpu_id, &mask);

        int ret = sched_setaffinity(0, sizeof(mask), &mask);

        if (ret != 0)
        {
            log::error() << "Couldn't set cpu affinity";
        }
    }

    static bool isset(std::size_t cpu_id)
    {
        cpu_set_t mask;
        sched_getaffinity(0, sizeof(mask), &mask);

        return CPU_ISSET(cpu_id, &mask);
    }
};
} // namespace os
} // namespace lo2s
