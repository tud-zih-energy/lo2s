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
#include "mmap.hpp"
#include "line_info.hpp"
#include "util.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <mutex>
#include <regex>
#include <utility>

namespace lo2s
{

memory_map::memory_map()
{
}

memory_map::memory_map(pid_t pid, bool read_initial)
{
    if (!read_initial)
    {
        return;
    }
    // Supposedly this one is faster than /proc/%d/maps for processes with many threads
    auto filename = str(fmt("/proc/%d/task/%d/maps") % pid % pid);

    std::ifstream mapstream(filename);
    if (mapstream.fail())
    {
        log::error() << "could not open maps file " << filename;
        // Gracefully return an initially empty map that always fails.
        return;
    }
    std::string line;

    //                 start       -    end          prot           offset
    //                 device  inode   dso
    std::regex regex("([0-9a-f]+)\\-([0-9a-f]+)\\s+[r-][w-]x.?\\s+([0-9a-z]+)"
                     "\\s+\\S+\\s+\\d+\\s+(.*)");
    log::debug() << "opening " << filename;
    while (getline(mapstream, line))
    {
        log::trace() << "map entry: " << line;
        std::smatch match;
        if (std::regex_match(line, match, regex))
        {
            auto begin = address(match.str(1));
            auto end = address(match.str(2));
            auto pgoff = address(match.str(3));
            auto filename = match.str(4);
            mmap(begin, end, pgoff, filename);
        }
    }
}

void memory_map::mmap(address begin, address end, address pgoff, const std::string& dso_name)
{
    if (dso_name.empty() || std::string("//anon") == dso_name ||
        std::string("/dev/zero") == dso_name || std::string("/anon_hugepage") == dso_name ||
        boost::starts_with(dso_name, "/SYSV"))
    {
        log::debug() << "mmap: skipping " << dso_name;
        return;
    }
    log::debug() << "mmap: " << begin << "-" << end << " " << pgoff << ": " << dso_name;

    auto ex_it = map_.find(begin);
    if (ex_it != map_.end())
    {
        // Overlapping with existing entry
        auto ex_range = ex_it->first;
        auto ex_elem = std::move(ex_it->second);
        map_.erase(ex_it);
        if (ex_range.start != begin)
        {
            // Truncate entry
            assert(ex_range.start < begin);
            ex_range.end = begin;
            log::debug() << "truncating map entry at " << ex_range.start << " to " << ex_range.end;
            auto r = map_.emplace(ex_range, std::move(ex_elem));
            (void)r;
            assert(r.second);
        }
    }

    try
    {
        binary* lb;
        if (dso_name[0] == '[')
        {
            lb = &named_binary::cache(dso_name);
        }
        else
        {
            lb = &bfd_radare_binary::cache(dso_name);
        }
        auto r = map_.emplace(std::piecewise_construct, std::forward_as_tuple(begin, end),
                              std::forward_as_tuple(begin, end, pgoff, *lb));
        if (!r.second)
        {
            // very common, so only debug
            // TODO handle better
            log::debug() << "duplicate memory range from mmap event. new: " << begin << "-" << end
                         << "%" << pgoff << " " << dso_name << "\n"
                         << "OLD: " << r.first->second.start << "-" << r.first->second.end << "%"
                         << r.first->second.pgoff << " " << r.first->second.dso.name();
        }
    }
    catch (bfdr::init_error& e)
    {
        log::info() << "could not initialize bfd: " << e.what();
    }
    catch (range::error& e)
    {
        // Very common, can't warn here
        // TODO consider handling this somehow...
        log::debug() << "invalid address range in /proc/.../maps: " << e.what();
    }
}

line_info memory_map::lookup_line_info(address ip) const
{
    try
    {
        auto& mapping = map_.at(ip);
        return mapping.dso.lookup_line_info(ip - mapping.start + mapping.pgoff);
    }
    catch (std::out_of_range&)
    {
        // This will just happen a lot in practice
        log::debug() << "no mapping found for address " << ip;
        // Graceful fallback
        return line_info(ip.truncate_bits(48));
    }
}

std::string memory_map::lookup_instruction(address ip) const
{
    auto& mapping = map_.at(ip);
    return mapping.dso.lookup_instruction(ip - mapping.start + mapping.pgoff);
}
}
