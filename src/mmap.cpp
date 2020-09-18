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

#include <lo2s/line_info.hpp>
#include <lo2s/mmap.hpp>
#include <lo2s/util.hpp>

#include <nitro/lang/string.hpp>

#include <fmt/core.h>

#include <mutex>
#include <regex>
#include <utility>

namespace lo2s
{

MemoryMap::MemoryMap()
{
}

MemoryMap::MemoryMap(pid_t pid, bool read_initial)
{
    if (!read_initial)
    {
        return;
    }
    // Supposedly this one is faster than /proc/%d/maps for processes with many threads
    auto filename = fmt::format("/proc/{}/task/{}/maps", pid, pid);

    std::ifstream mapstream(filename);
    if (mapstream.fail())
    {
        Log::error() << "could not open maps file " << filename;
        // Gracefully return an initially empty map that always fails.
        return;
    }
    std::string line;

    //                 start       -    end          prot           offset
    //                 device  inode   dso
    // NOTE: we only look at executable entries       here ↓
    std::regex regex("([0-9a-f]+)\\-([0-9a-f]+)\\s+[r-][w-]x.?\\s+([0-9a-z]+)"
                     "\\s+\\S+\\s+\\d+\\s+(.*)");
    Log::debug() << "opening " << filename;
    while (getline(mapstream, line))
    {
        Log::trace() << "map entry: " << line;
        std::smatch match;
        if (std::regex_match(line, match, regex))
        {
            RawMemoryMapEntry entry(Address(match.str(1)), Address(match.str(2)),
                                    Address(match.str(3)), match.str(4));
            mmap(entry);
        }
    }
}

void MemoryMap::mmap(const RawMemoryMapEntry& entry)
{
    Log::debug() << "mmap: " << entry.addr << "-" << entry.end << " " << entry.pgoff << ": "
                 << entry.filename;

    if (entry.filename.empty() || std::string("//anon") == entry.filename ||
        std::string("/dev/zero") == entry.filename ||
        std::string("/anon_hugepage") == entry.filename ||
        nitro::lang::starts_with(entry.filename, "/SYSV"))
    {
        Log::debug() << "mmap: skipping dso: " << entry.filename << " (known non-library)";
        return;
    }

    bool is_non_file_dso = (entry.filename[0] == '[');

    Binary* lb;
    if (is_non_file_dso)
    {
        lb = &NamedBinary::cache(entry.filename);
    }
    else
    {
        try
        {

            lb = &BfdRadareBinary::cache(entry.filename);
        }
        catch (bfdr::InitError& e)
        {
            Log::warn() << "could not initialize bfd: " << e.what();
            lb = &NamedBinary::cache(entry.filename);
        }
        catch (bfdr::InvalidFileError& e)
        {
            Log::debug() << "dso is not a valid file: " << e.what();
            lb = &NamedBinary::cache(entry.filename);
        }
    }

    auto ex_it = map_.find(entry.addr);
    if (ex_it != map_.end())
    {
        // Overlapping with existing entry
        auto ex_range = ex_it->first;
        auto ex_elem = std::move(ex_it->second);
        map_.erase(ex_it);
        if (ex_range.start != entry.addr)
        {
            // Truncate entry
            assert(ex_range.start < entry.addr);
            ex_range.end = entry.addr;
            Log::debug() << "truncating map entry at " << ex_range.start << " to " << ex_range.end;
            [[maybe_unused]] auto r = map_.emplace(ex_range, std::move(ex_elem));
            assert(r.second);
        }
    }

    try
    {
        auto r =
            map_.emplace(std::piecewise_construct, std::forward_as_tuple(entry.addr, entry.end),
                         std::forward_as_tuple(entry.addr, entry.end, entry.pgoff, *lb));
        if (!r.second)
        {
            // very common, so only debug
            // TODO handle better
            Log::debug() << "duplicate memory range from mmap event. new: " << entry.addr << "-"
                         << entry.end << "%" << entry.pgoff << " " << entry.filename << "\n"
                         << "OLD: " << r.first->second.start << "-" << r.first->second.end << "%"
                         << r.first->second.pgoff << " " << r.first->second.dso.name();
        }
    }
    catch (Range::Error& e)
    {
        // Very common, can't warn here
        // TODO consider handling this somehow...
        Log::debug() << "invalid address range in /proc/.../maps: " << e.what();
    }
}

LineInfo MemoryMap::lookup_line_info(Address ip) const
{
    try
    {
        auto& mapping = map_.at(ip);
        return mapping.dso.lookup_line_info(ip - mapping.start + mapping.pgoff);
    }
    catch (std::out_of_range&)
    {
        // This will just happen a lot in practice
        Log::trace() << "no mapping found for address " << ip;
        // Graceful fallback
        return LineInfo::for_unknown_function();
    }
}

std::string MemoryMap::lookup_instruction(Address ip) const
{
    auto& mapping = map_.at(ip);
    return mapping.dso.lookup_instruction(ip - mapping.start + mapping.pgoff);
}
} // namespace lo2s
