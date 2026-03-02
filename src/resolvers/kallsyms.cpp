// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/resolvers/kallsyms.hpp>

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/log.hpp>

#include <fstream>
#include <map>
#include <regex>
#include <string>
#include <tuple>
#include <utility>

#include <cstdint>

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
    std::regex const ksym_regex("([0-9a-f]+) (?:t|T) ([^[:space:]]+)");
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

    if (!sym_str.empty())
    {
        kallsyms_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(prev, Address(UINT64_MAX)),
                          std::forward_as_tuple(sym_str));
    }
}
} // namespace lo2s
