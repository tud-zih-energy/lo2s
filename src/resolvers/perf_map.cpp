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

#include <lo2s/resolvers/perf_map.hpp>

#include <lo2s/address.hpp>
#include <lo2s/function_resolver.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/types/process.hpp>

#include <fstream>
#include <regex>
#include <string>

#include <cstdint>

#include <fmt/format.h>

namespace lo2s
{
PerfMap::PerfMap(Process process) : FunctionResolver(fmt::format("JIT functions for {}", process))
{
    std::ifstream perf_map_file(fmt::format("/tmp/perf-{}.map", process.as_int()));

    // Entries in the perf-[PID].map file have the form:
    // START SIZE SYMBOL
    // Example:
    // ff00ff deadbeef py::foo
    std::regex const perf_map_regex("([0-9a-f]+) ([0-9a-f]+) (.+)");

    // python mapfiles have the symbols in the form
    // py::[functioname]:[filename]
    std::regex const python_regex("py::(.+):(.+)");

    std::string line;
    while (std::getline(perf_map_file, line))
    {

        std::smatch perf_map_match;
        if (std::regex_match(line, perf_map_match, perf_map_regex))
        {
            LineInfo info = LineInfo::for_unknown_function();
            Address const start(perf_map_match[1]);

            if (start_ == UINT64_MAX)
            {
                start_ = start;
            }

            Address const end(start + Address(perf_map_match[2]));

            std::smatch python_match;

            std::string const symbol_string = perf_map_match[3].str();

            if (std::regex_match(symbol_string, python_match, python_regex))
            {
                // symbols where the filename starts with '<' (e.g '<frozen os>')
                // are compiled into the Python interpreter
                if (python_match[2].str()[0] == '<')
                {
                    std::string const dso = python_match[2];
                    std::string const function = python_match[1];
                    info = LineInfo::for_function("<unknown file>", function.c_str(), 0, dso);
                }
                else
                {
                    std::string const function = python_match[1];
                    std::string const filename = python_match[2];
                    info = LineInfo::for_function(filename.c_str(), function.c_str(), 0, filename);
                }
            }
            else
            {
                info = LineInfo::for_function("<unknown file>", symbol_string.c_str(), 0, "<jit>");
            }

            entries_.emplace(Range(start, end), info);
        }
    }

    if (!entries_.empty())
    {
        end_ = (--entries_.end())->first.end;
    }
}
} // namespace lo2s
