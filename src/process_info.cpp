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
    try
    {

        std::shared_ptr<FunctionResolver> kall = Kallsyms::cache();

        auto& fr_map = function_resolvers_
                           .emplace(MeasurementScope::sample(p.as_scope()),
                                    std::map<Mapping, std::shared_ptr<FunctionResolver>>())
                           .first->second;

        fr_map.emplace(std::piecewise_construct,
                       std::forward_as_tuple(Kallsyms::cache()->start(), (uint64_t)-1, 0),
                       std::forward_as_tuple(kall));
    }
    catch (std::exception& e)
    {
        Log::debug() << "Could not read kallsyms: " << e.what();
    }
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

void ProcessInfo::emplace_fr(Address addr, Address end, Address pgoff, std::string filename)
{
    std::shared_ptr<FunctionResolver> fr;
    try
    {
        if (nitro::lang::starts_with(filename, "["))
        {
            fr = FunctionResolver::cache(filename);
        }
        else
        {
            fr = DwarfFunctionResolver::cache(filename);
        }
    }
    catch (std::exception& e)
    {
        fr = FunctionResolver::cache(filename);
    }

    if (fr == nullptr)
    {
        return;
    }
    auto& fr_map = function_resolvers_
                       .emplace(MeasurementScope::sample(process_.as_scope()),
                                std::map<Mapping, std::shared_ptr<FunctionResolver>>())
                       .first->second;

    auto fr_entry = fr_map.find(Mapping(addr));
    if (fr_entry != fr_map.end())
    {
        // Overlapping with existing entry
        auto fr_mapping = fr_entry->first;
        auto fr = std::move(fr_entry->second);
        fr_map.erase(fr_entry);
        if (fr_mapping.range.start != addr)
        {
            // Truncate entry
            assert(fr_mapping.range.start < addr);
            fr_mapping.range.end = addr;
            Log::debug() << "truncating map entry at " << fr_mapping.range.start << " to "
                         << fr_mapping.range.end;
            [[maybe_unused]] auto r = fr_map.emplace(fr_mapping, std::move(fr));
            assert(r.second);
        }
    }

    try
    {
        fr_map.emplace(std::piecewise_construct, std::forward_as_tuple(addr, end, pgoff),
                       std::forward_as_tuple(fr));
    }
    catch (Range::Error& e)
    {
        // Very common, can't warn here
        // TODO consider handling this somehow...
        Log::debug() << "invalid address range in /proc/.../maps: " << e.what();
    }
}

void ProcessInfo::emplace_ir(Address addr, Address end, Address pgoff, std::string filename)
{
    std::shared_ptr<InstructionResolver> ir = nullptr;
#ifdef HAVE_RADARE
    try
    {
        ir = RadareInstructionResolver::cache(filename);
    }
    catch (...)
    {
    }
#endif

    if (ir == nullptr)
    {
        return;
    }

    auto& ir_map = instruction_resolvers_
                       .emplace(MeasurementScope::sample(process_.as_scope()),
                                std::map<Mapping, std::shared_ptr<InstructionResolver>>())
                       .first->second;

    auto ir_entry = ir_map.find(Mapping(addr));
    if (ir_entry != ir_map.end())
    {
        // Overlapping with existing entry
        auto ir_mapping = ir_entry->first;
        auto ir = std::move(ir_entry->second);
        ir_map.erase(ir_entry);
        if (ir_mapping.range.start != addr)
        {
            // Truncate entry
            assert(ir_mapping.range.start < addr);
            ir_mapping.range.end = addr;
            Log::debug() << "truncating map entry at " << ir_mapping.range.start << " to "
                         << ir_mapping.range.end;
            [[maybe_unused]] auto r = ir_map.emplace(ir_mapping, std::move(ir));
            assert(r.second);
        }
    }

    try
    {
        ir_map.emplace(std::piecewise_construct, std::forward_as_tuple(addr, end, pgoff),
                       std::forward_as_tuple(ir));
    }
    catch (Range::Error& e)
    {
        // Very common, can't warn here
        // TODO consider handling this somehow...
        Log::debug() << "invalid address range in /proc/.../maps: " << e.what();
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

    emplace_fr(addr, end, pgoff, filename);
    emplace_ir(addr, end, pgoff, filename);
}

LineInfo ProcessInfo::lookup_line_info(MeasurementScope scope, Address ip)
{
    try
    {
        std::shared_lock lock(mutex_);

        auto& fr_map = function_resolvers_.at(scope);
        for (auto& fr : fr_map)
        {
            if (ip >= fr.first.range.start && ip < fr.first.range.end)
            {
                return fr.second->lookup_line_info(ip - fr.first.range.start + fr.first.pgoff);
            }
        }
    }
    catch (std::exception& e)
    {
        Log::debug() << process_ << ": Could not find mapping for ip: " << ip << e.what();
    }
    return LineInfo::for_unknown_function();
}

std::string ProcessInfo::lookup_instruction(MeasurementScope scope, Address ip) const
{
    auto& ir_map = instruction_resolvers_.at(scope);

    auto ir = ir_map.find(Mapping(ip));
    if (ir != ir_map.end())
    {
        return ir->second->lookup_instruction(ip - ir->first.range.start + ir->first.pgoff);
    }
    return "";
}

bool ProcessMap::has(Process p)
{
    return infos_.count(p) != 0;
}

ProcessInfo& ProcessMap::get(Process p)
{
    return infos_.at(p);
}

ProcessInfo& ProcessMap::insert(Process p, bool read_initial)
{
    if (infos_.count(p))
    {
        infos_.erase(p);
    }
    auto it = infos_.emplace(std::piecewise_construct, std::forward_as_tuple(p),
                             std::forward_as_tuple(p, read_initial));
    return it.first->second;
}
} // namespace lo2s
