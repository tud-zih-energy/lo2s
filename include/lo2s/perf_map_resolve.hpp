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

#include <fmt/core.h>
#include <lo2s/address.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/log.hpp>

namespace lo2s
{
class PerfMap
{
public:
    PerfMap(pid_t pid)
    {
        Log::error() << "Opening Perf Map for: " << pid;
        std::ifstream perf_map_file(fmt::format("/tmp/perf-{}.map", pid));

        std::string line;
        while (std::getline(perf_map_file, line))
        {
            // Entries in the perf-[PID].map file have the form:
            // START SIZE SYMBOL
            // Example:
            // ff00ff deadbeef py::foo
            std::regex perf_map_regex("([0-9a-f]+) ([0-9a-f]+) (.+)");
            std::smatch perf_map_match;

            if (std::regex_match(line, perf_map_match, perf_map_regex))
            {
                LineInfo info = LineInfo::for_unknown_function();
                Address start(perf_map_match[1]);
                Address end(start + Address(perf_map_match[2]));

                // python mapfiles have the symbols in the form
                // py::[functioname]:[filename]
                std::regex python_regex("py::(.+):(.+)");
                std::smatch python_match;

                std::string symbol_string = perf_map_match[3].str();

                if (std::regex_match(symbol_string, python_match, python_regex))
                {
                    // symbols where the filename starts with '<' (e.g '<frozen os>')
                    // are compiled into the Python interpreter
                    if (python_match[2].str()[0] == '<')
                    {
                        std::string dso = python_match[2];
                        std::string function = python_match[1];
                        info = LineInfo("<unknown file>", function, 0, dso);
                    }
                    else
                    {
                        std::string function = python_match[1];
                        std::string filename = python_match[2];
                        info = LineInfo(filename, function, 0, filename);
                    }
                }
                else
                {
                    info = LineInfo("<unknown file>", symbol_string, 0, "<jit>");
                }
                symbols_.emplace(Range(start, end), info);
            }
        }
    }

    LineInfo lookup(Address addr) const
    {
        for (const auto& symbol : symbols_)
        {
            if (symbol.first.in(addr))
            {
                return symbol.second;
            }
            if (symbol.first.end > addr)
            {
                break;
            }
        }
        return LineInfo::for_unknown_function();
    }

private:
    std::map<Range, LineInfo> symbols_;
};

class PerfMapCache
{
public:
    static PerfMapCache& instance()
    {
        static PerfMapCache cache;
        return cache;
    }

    LineInfo lookup_line_info(pid_t pid, Address addr)
    {
        Log::error() << "Looking up: " << addr;
        if (maps_.count(pid))
        {
            return maps_.at(pid).lookup(addr);
        }
        else
        {
            auto res = maps_.emplace(std::piecewise_construct, std::forward_as_tuple(pid),
                                     std::forward_as_tuple(pid));

            return res.first->second.lookup(addr);
        }
    }

private:
    PerfMapCache()
    {
    }

    std::map<pid_t, PerfMap> maps_;
};
} // namespace lo2s
