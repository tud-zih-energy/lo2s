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
#include <lo2s/dwarf_resolve.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/instruction_resolver.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/types.hpp>

#include <nitro/lang/string.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <fstream>
#include <shared_mutex>
#include <thread>

namespace lo2s
{

// Mapping of memory ranges to "class T" objects
//
// Used for recording various information for the different executable files mapped into a program
template <class T>
class MemoryMap
{
public:
    MemoryMap()
    {
    }

    MemoryMap(const MemoryMap&) = default;
    MemoryMap& operator=(const MemoryMap&) = default;
    MemoryMap(const MemoryMap&&) = delete;
    MemoryMap& operator=(const MemoryMap&&) = delete;

    void emplace(Mapping new_mapping, std::shared_ptr<T> to_emplace)
    {
        /*
         * Mappings should not overlap in the final data structure.
         * However, a `new_mapping` can overlap with existing mappings in three ways:
         *  1. One or multiple existing mapping(s) is/are completely inside the new mapping
         *  2. The new mapping is completely inside an existing mapping
         *  2. There new mapping overlaps partially with existing mappings.
         */

        /*
         * 1. Check for existing mappings that are inside the new mapping
         *
         * If they exist, delete them
         */
        for (auto it = map_.begin(); it != map_.end();)
        {
            if (it->first.in(new_mapping))
            {
                Log::debug() << fmt::format("{}", it->first.range) << " in "
                             << fmt::format("{}", new_mapping.range) << "deleting!";

                it = map_.erase(it);
            }
            else
            {
                it++;
            }
        }

        /* 2. Check if the new_mapping is completely in an existing mapping
         *
         */
        auto inside_it = std::find_if(map_.begin(), map_.end(), [&new_mapping](auto& elem) {
            return new_mapping.in(elem.first);
        });

        if (inside_it != map_.end())
        {
            // If this is the case:
            Log::debug() << fmt::format("{}", new_mapping.range) << " completely in "
                         << fmt::format("{}", inside_it->first.range) << ", splitting!";

            // 1. Save the value of the existing mapping
            Mapping existing_mapping = inside_it->first;
            std::shared_ptr<T> content = inside_it->second;

            // 2. Delete existing mapping
            map_.erase(inside_it);

            // 3. emplace [existing.start, new_mapping.start] -> existing value
            Mapping first_old_mapping(existing_mapping.range.start, new_mapping.range.start,
                                      existing_mapping.pgoff);
            [[maybe_unused]] auto r = map_.emplace(first_old_mapping, content);
            assert(r.second);

            //  4. emplace [new_mapping.start, new_mapping.end] -> new value
            r = map_.emplace(new_mapping, to_emplace);
            assert(r.second);

            //  5. emplace [new_mapping.end, existing_mapping.end] -> existing value
            Mapping second_old_mapping(new_mapping.range.end, existing_mapping.range.end,
                                       existing_mapping.pgoff);
            r = map_.emplace(second_old_mapping, content);
            assert(r.second);

            return;
        }

        // 4. Check for partial overlaps, truncating the beginning/ending of existing mappings
        // 4.1 Partial overlaps at the start of the new mapping
        auto start_entry = map_.find(new_mapping.range.start);
        if (start_entry != map_.end())
        {
            Mapping existing_mapping = start_entry->first;
            std::shared_ptr<T> content = start_entry->second;

            map_.erase(start_entry);

            Log::debug() << "moving map entry end from " << existing_mapping.range.end << " to "
                         << new_mapping.range.start;

            existing_mapping.range.end = existing_mapping.range.start;

            [[maybe_unused]] auto r = map_.emplace(existing_mapping, content);
            assert(r.second);
        }

        // 4.2 Partial overlaps at the end of the mapping
        auto end_entry = map_.find(new_mapping.range.end);
        if (end_entry != map_.end())
        {
            Mapping existing_mapping = end_entry->first;
            std::shared_ptr<T> content = end_entry->second;

            map_.erase(end_entry);

            Log::debug() << "moving map entry start from " << existing_mapping.range.start << " to "
                         << new_mapping.range.end;
            existing_mapping.range.start = new_mapping.range.end;

            [[maybe_unused]] auto r = map_.emplace(existing_mapping, content);
            assert(r.second);
        }

        map_.emplace(std::piecewise_construct, std::forward_as_tuple(new_mapping),
                     std::forward_as_tuple(to_emplace));
    }

    typename std::map<Mapping, std::shared_ptr<T>>::iterator find(const Address& addr)
    {
        return map_.find(addr);
    }

    typename std::map<Mapping, std::shared_ptr<T>>::const_iterator find(const Address& addr) const
    {
        return map_.find(addr);
    }

    bool has(const Address& addr) const
    {
        return map_.count(addr) != 0;
    }

    typename std::map<Mapping, std::shared_ptr<T>>::iterator end()
    {
        return map_.end();
    }

    typename std::map<Mapping, std::shared_ptr<T>>::const_iterator end() const
    {
        return map_.end();
    }

private:
    std::map<Mapping, std::shared_ptr<T>> map_;
};

// Specialization of MemoryMap for FunctionResolvers.
// This takes care of emplacing the Kallsyms resolver for kernel symbols
class ProcessFunctionMap : public MemoryMap<FunctionResolver>
{
public:
    ProcessFunctionMap(Process process)
    {
        auto kall = Kallsyms::cache();
        emplace(Mapping(Kallsyms::cache()->start(), (uint64_t)-1, 0), kall);
        auto perf_map = PerfMap::cache(process);
        emplace(perf_map->mapping(), perf_map);
    }
};

} // namespace lo2s
