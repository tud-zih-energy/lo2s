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

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/log.hpp>
#include <lo2s/mmap.hpp>

#include <mutex>
#include <thread>

namespace lo2s
{
class ProcessInfo
{
public:
    ProcessInfo(Process p, bool enable_on_exec) : process_(p), maps_(p, !enable_on_exec)
    {
    }

    Process process() const
    {
        return process_;
    }

    void mmap(const RawMemoryMapEntry& entry)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        maps_.mmap(entry);
    }

    MemoryMap maps() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Yes, this is correct as per 6.6.3 The Return Statement
        return maps_;
    }

private:
    const Process process_;
    mutable std::mutex mutex_;
    MemoryMap maps_;
};
} // namespace lo2s
