/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
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

#include <lo2s/process_info.hpp>

namespace lo2s
{

ProcessInfo::ProcessInfo(Process p, bool read_initial) : process_(p)
{
    FunctionResolver& kall = Kallsyms::cache();
    InstructionResolver& kernel_ir = InstructionResolver::cache();
    map_.emplace(
        std::piecewise_construct, std::forward_as_tuple(Kallsyms::cache().start(), (uint64_t)-1),
        std::forward_as_tuple(Kallsyms::cache().start(), (uint64_t)-1, 0, kall, kernel_ir));
    if (!read_initial)
    {
        return;
    }
    // Supposedly this one is faster than /proc/%d/maps for processes with many threads
    auto filename = fmt::format("/proc/{}/task/{}/maps", p.as_pid_t(), p.as_pid_t());

    std::ifstream mapstream(filename);
    if (mapstream.fail())
    {
        Log::error() << "could not open maps file " << filename;
        // Gracefully return an initially empty map that always fails.
        return;
    }

    std::string line;

    //                 start       -    end          prot           offset
    //                 device  inode   fr
    // NOTE: we only look at executable entries       here ↓
    std::regex regex("([0-9a-f]+)\\-([0-9a-f]+)\\s+[r-][w-][x-].?\\s+([0-9a-z]+)"
                     "\\s+\\S+\\s+\\d+\\s+(.*)");
    Log::debug() << "opening " << filename;
    while (getline(mapstream, line))
    {
        Log::trace() << "map entry: " << line;
        std::smatch match;
        if (std::regex_match(line, match, regex))
        {
            mmap(Address(match.str(1)), Address(match.str(2)), Address(match.str(3)), match.str(4));
        }
    }
}

void ProcessInfo::mmap(Address addr, Address end, Address pgoff, std::string filename)
{

    std::lock_guard lock(mutex_);

    Log::debug() << "mmap: " << addr << "-" << end << " " << pgoff << ": " << filename;

    if (filename.empty() || std::string("//anon") == filename ||
        std::string("/dev/zero") == filename || std::string("/anon_hugepage") == filename ||
        nitro::lang::starts_with(filename, "/memfd") ||
        nitro::lang::starts_with(filename, "/SYSV") || nitro::lang::starts_with(filename, "/dev"))
    {
        Log::debug() << "mmap: skipping fr: " << filename << " (known non-library)";
        return;
    }

    FunctionResolver* fr;
    InstructionResolver* ir;

    try
    {
        if (nitro::lang::starts_with(filename, "["))
        {
            fr = &FunctionResolver::cache(filename);
        }
        else
        {
            fr = &DwarfFunctionResolver::cache(filename);
        }
    }
    catch (...)
    {
        fr = &FunctionResolver::cache(filename);
    }

#ifdef HAVE_RADARE
    try
    {
        ir = &RadareInstructionResolver::cache(filename);
    }
    catch (...)
    {
        ir = &InstructionResolver::cache();
    }
#else
    ir = &InstructionResolver::cache();
#endif
    auto ex_it = map_.find(addr);
    if (ex_it != map_.end())
    {
        // Overlapping with existing entry
        auto ex_range = ex_it->first;
        auto ex_elem = std::move(ex_it->second);
        map_.erase(ex_it);
        if (ex_range.start != addr)
        {
            // Truncate entry
            assert(ex_range.start < addr);
            ex_range.end = addr;
            Log::debug() << "truncating map entry at " << ex_range.start << " to " << ex_range.end;
            [[maybe_unused]] auto r = map_.emplace(ex_range, std::move(ex_elem));
            assert(r.second);
        }
    }

    try
    {
        auto r = map_.emplace(std::piecewise_construct, std::forward_as_tuple(addr, end),
                              std::forward_as_tuple(addr, end, pgoff, *fr, *ir));
        if (!r.second)
        {
            // very common, so only debug
            // TODO handle better
            Log::debug() << "duplicate memory range from mmap event. new: " << addr << "-" << end
                         << "%" << pgoff << " " << filename << "\n"
                         << "OLD: " << r.first->second.start << "-" << r.first->second.end << "%"
                         << r.first->second.pgoff << " " << r.first->second.fr.name();
        }
    }
    catch (Range::Error& e)
    {
        // Very common, can't warn here
        // TODO consider handling this somehow...
        Log::debug() << "invalid address range in /proc/.../maps: " << e.what();
    }
}

LineInfo ProcessInfo::lookup_line_info(Address ip)
{
    try
    {
        std::shared_lock lock(mutex_);
        return map_.at(ip).fr.lookup_line_info(ip - map_.at(ip).start + map_.at(ip).pgoff);
    }
    catch (std::exception& e)
    {
        Log::error() << process_ << ": Could not find mapping for ip: " << ip << e.what();
        return LineInfo::for_unknown_function();
    }
}

std::string ProcessInfo::lookup_instruction(Address ip) const
{
    return map_.at(ip).ir.lookup_instruction(ip - map_.at(ip).start + map_.at(ip).pgoff);
}

bool ProcessMap::has(Process p, uint64_t timepoint)
{
    auto it = infos_.find(p);
    if (it == infos_.end())
    {
        return false;
    }
    for (auto& elem : it->second)
    {
        if (timepoint >= elem.first)
        {
            return true;
        }
    }
    return false;
}

ProcessInfo& ProcessMap::get(Process p, uint64_t timepoint)
{
    auto& infos = infos_.at(p);

    auto it = std::find_if(infos.begin(), infos.end(),
                           [&timepoint](auto& arg) { return arg.first > timepoint; });

    if (it == infos.begin())
    {
        if (timepoint >= it->first)
        {
            return it->second;
        }
        else
        {
            throw std::out_of_range("no matching timepoint");
        }
    }
    else
    {
        it--;
        if (timepoint >= it->first)
        {
            return it->second;
        }
        else
        {
            throw std::out_of_range("no matching timepoint it--");
        }
    }

    return infos_.at(p).at(timepoint);
}

ProcessInfo& ProcessMap::insert(Process p, uint64_t timepoint, bool read_initial)
{
    auto it = infos_[p].emplace(std::piecewise_construct, std::forward_as_tuple(timepoint),
                                std::forward_as_tuple(p, read_initial));
    return it.first->second;
}
} // namespace lo2s
