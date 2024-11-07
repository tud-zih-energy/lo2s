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
#include <lo2s/function_resolver.hpp>
#include <lo2s/instruction_resolver.hpp>
#include <lo2s/dwarf_resolve.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/types.hpp>

#include <nitro/lang/string.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <fstream>
#include <shared_mutex>
#include <thread>

namespace lo2s
{
class ProcessInfo
{
public:
    ProcessInfo(Process p, bool read_initial);
    Process process() const
    {
        return process_;
    }

    void mmap(const RecordMmap2Type& entry)
    {
        mmap(entry.addr, entry.addr + entry.len, entry.pgoff, entry.filename);
    }

    void mmap(Address addr, Address end, Address pgoff, std::string filename);

    LineInfo lookup_line_info(Address ip);
    std::string lookup_instruction(Address ip) const;

private:
    struct Mapping
    {
        Mapping(Address s, Address e, Address o, FunctionResolver& f, InstructionResolver& i)
        : start(s), end(e), pgoff(o), fr(f), ir(i)
        {
        }

        Address start;
        Address end;
        Address pgoff;
        FunctionResolver& fr;
        InstructionResolver& ir;
    };
    const Process process_;
    mutable std::shared_mutex mutex_;
    std::map<Range, Mapping> map_;
};

class ProcessMap
{
public:
    ProcessMap()
    {
    }

    bool has(Process p, uint64_t timepoint);

    ProcessInfo& get(Process p, uint64_t timepoint);

    ProcessInfo& insert(Process p, uint64_t timepoint, bool read_initial);

private:
    std::map<Process, std::map<uint64_t, ProcessInfo>> infos_;
};
} // namespace lo2s
