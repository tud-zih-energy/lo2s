/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2026,
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

#include <lo2s/resolvers/kallsyms.hpp>

#include <regex>

namespace lo2s
{
Kallsyms::Kallsyms() : FunctionResolver("[kernel]")
{
    std::map<Address, std::string> entries;
    std::ifstream ksyms_file;
    ksyms_file.exceptions(std::ifstream::badbit);
    ksyms_file.open("/proc/kallsyms");

    if (!ksyms_file.good())
    {
        Log::debug() << "Can not parse /proc/kallsyms, consider lowering perf_event_paranoid";
        return;
    }
    std::regex ksym_regex("([0-9a-f]+) (?:t|T) ([^[:space:]]+)");
    std::smatch ksym_match;

    std::string line;

    // Emplacing into entries map takes care of sorting symbols by address
    while (getline(ksyms_file, line))
    {
        if (std::regex_match(line, ksym_match, ksym_regex))
        {
            std::string sym_str = ksym_match[2];

            uint64_t sym_addr = stoull(ksym_match[1], nullptr, 16); // NOLINT

            // If perf_event_paranoid is not sufficient enough, all symbols show up as 0
            if (sym_addr == 0)
            {
                Log::debug()
                    << "Can not parse /proc/kallsyms, consider lowering perf_event_paranoid";
                return;
            }
            entries.emplace(std::piecewise_construct, std::forward_as_tuple(sym_addr),
                            std::forward_as_tuple(sym_str));
        }
    }

    std::string sym_str;
    Address prev = 0;

    for (auto& entry : entries)
    {
        if (prev != 0 && prev != entry.first)
        {
            kallsyms_.emplace(std::piecewise_construct, std::forward_as_tuple(prev, entry.first),
                              std::forward_as_tuple(sym_str));
        }
        else
        {
            start_ = entry.first.value();
        }
        sym_str = entry.second;
        prev = entry.first;
    }

    if (sym_str != "")
    {
        kallsyms_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(prev, Address(UINT64_MAX)),
                          std::forward_as_tuple(sym_str));
    }
}
} // namespace lo2s
