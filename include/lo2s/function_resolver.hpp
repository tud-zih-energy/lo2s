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

#pragma once

#include <lo2s/line_info.hpp>
#include <lo2s/util.hpp>

#include <regex>
#include <string>

namespace lo2s
{
class FunctionResolver
{
public:
    FunctionResolver(const std::string& name) : name_(name)
    {
    }

    static std::shared_ptr<FunctionResolver> cache(const std::string& name)
    {
        return BinaryCache<FunctionResolver>::instance()[name];
    }

    virtual LineInfo lookup_line_info(Address)
    {
        return LineInfo::for_binary(name_);
    }

    std::string name()
    {
        return name_;
    }

    virtual ~FunctionResolver()
    {
    }

protected:
    std::string name_;
};

class Kallsyms : public FunctionResolver
{
public:
    Kallsyms() : FunctionResolver("[kernel]")
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
                uint64_t sym_addr = stoull(ksym_match[1], nullptr, 16);

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

        std::string sym_str = "";
        Address prev = 0;

        for (auto& entry : entries)
        {
            if (prev != 0 && prev != entry.first)
            {
                kallsyms_.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(prev, entry.first),
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

    static std::shared_ptr<Kallsyms> cache()
    {
        static std::shared_ptr<Kallsyms> k = std::make_shared<Kallsyms>();
        return k;
    }

    uint64_t start()
    {
        return start_;
    }

    virtual LineInfo lookup_line_info(Address addr) override
    {
        auto it = kallsyms_.find(addr + start_);
        if (it != kallsyms_.end())
        {
            return LineInfo::for_function("", it->second.c_str(), 1, "[kernel]");
        }
        else
        {
            return LineInfo::for_binary("[kernel]");
        }
    }

private:
    std::map<Range, std::string> kallsyms_;
    uint64_t start_ = UINT64_MAX;
};

std::shared_ptr<FunctionResolver> function_resolver_for(const std::string& filename);
} // namespace lo2s
