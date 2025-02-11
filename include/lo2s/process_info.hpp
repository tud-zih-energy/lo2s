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
class ProcessInfo
{
public:
    ProcessInfo(Process p, bool read_initial);
    Process process() const
    {
        return process_;
    }

    void mmap(const RecordMmapType& entry)
    {
        mmap(entry.addr, entry.addr + entry.len, entry.pgoff, entry.filename);
    }

    void mmap(Address addr, Address end, Address pgoff, std::string filename);

    void insert_functions(MeasurementScope scope, std::map<Address, std::string> functions);

    LineInfo lookup_line_info(MeasurementScope scope, Address ip);
    std::string lookup_instruction(MeasurementScope scope, Address ip) const;

private:
    void emplace_fr(Address addr, Address end, Address pgoff, std::string filename);
    void emplace_ir(Address addr, Address end, Address pgoff,
                    std::string filename [[maybe_unused]]);

    struct Mapping
    {
        Mapping(Address s, Address e, Address o) : range(s, e), pgoff(o)
        {
        }

        Mapping(Address s) : range(s), pgoff(0)
        {
        }

        Range range;
        Address pgoff;

        bool operator==(const Address& other) const
        {
            return other >= range.start && other < range.end;
        }

        bool operator<(const Mapping& other) const
        {
            return range < other.range;
        }

        bool operator<(const Address& other) const
        {
            return range < other;
        }

        static Mapping max()
        {
            return Mapping(0, (uint64_t)-1, 0);
        }
    };

    const Process process_;
    mutable std::shared_mutex mutex_;
    std::map<MeasurementScope, std::map<Mapping, std::shared_ptr<FunctionResolver>>>
        function_resolvers_;
    std::map<MeasurementScope, std::map<Mapping, std::shared_ptr<InstructionResolver>>>
        instruction_resolvers_;
};

class ProcessMap
{
public:
    ProcessMap()
    {
    }

    bool has(Process p);

    ProcessInfo& get(Process p);

    ProcessInfo& insert(Process p, bool read_initial);

private:
    std::map<Process, ProcessInfo> infos_;
};
} // namespace lo2s
