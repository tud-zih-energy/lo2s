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

#include <lo2s/process_map/process_map.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/process_map/process.hpp>

namespace lo2s
{

ProcessMap& process_map()
{
    static ProcessMap p;
    return p;
}

ProcessMap::ProcessMap()
{
}

Process &ProcessMap::get_process(pid_t pid)
{
    return processes.at(pid);
}

Thread &ProcessMap::get_thread(pid_t tid)
{
    return threads.at(tid);
}

void ProcessMap::insert(pid_t pid, pid_t tid)
{
    summary().add_thread();

    if(processes.count(pid) == 0)
    {
        processes.emplace(pid, Process());
    }
    if(threads.count(tid) == 0)
    {
        threads.emplace(tid, Thread(pid));
    }
}

void ProcessMap::erase(pid_t tid)
{
    processes.erase(tid);
    threads.erase(tid);
}
}
