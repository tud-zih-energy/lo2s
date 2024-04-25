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

#include <filesystem>

namespace lo2s
{
struct LineInfo
{
private:
    static constexpr unsigned int UNKNOWN_LINE = 0;

    // Note: If line is not known, we write 1 anyway so the rest is shown in vampir
    // phijor 2018-11-08: I think this workaround is not needed anymore? vampir
    // shows source code locations with line == 0 just fine.
    LineInfo(const std::string& file, const std::string& function, unsigned int line,
             const std::string& dso)
    : file(file), function(function), line((line != UNKNOWN_LINE) ? line : 1), dso(dso)
    {
    }

    LineInfo(const char* file, const char* function, unsigned int line, const std::string& dso)
    : file(file), function(function), line((line != UNKNOWN_LINE) ? line : 1), dso(dso)
    {
    }

public:
    static LineInfo for_function(const char* file, const char* function, unsigned int line,
                                 const std::string& dso)
    {
        return LineInfo((file != nullptr) ? file : "<unknown file>",
                        (function != nullptr) ? function : "<unknown function>", line,
                        std::filesystem::path(dso).filename().string());
    }

    static LineInfo for_unknown_function()
    {
        return LineInfo("<unknown file>", "<unknown function>", UNKNOWN_LINE, "<unknown binary>");
    }

    static LineInfo for_unknown_function_in_dso(const std::string& dso)
    {
        return LineInfo("<unknown file>", "<unknown function>", UNKNOWN_LINE,
                        std::filesystem::path(dso).filename().string());
    }

    static LineInfo for_binary(const std::string& binary)
    {
        return LineInfo(binary, binary, UNKNOWN_LINE, binary);
    }

    std::string file;
    std::string function;
    unsigned int line;
    std::string dso;

    // For std::map
    bool operator<(const LineInfo& other) const
    {
        return std::tie(file, function, line, dso) <
               std::tie(other.file, other.function, other.line, dso);
    }

    bool operator==(const LineInfo& other) const
    {
        return std::tie(file, function, line, dso) ==
               std::tie(other.file, other.function, other.line, dso);
    }
};

inline std::ostream& operator<<(std::ostream& os, const LineInfo& info)
{
    return os << info.function << " @ " << info.file << ":" << info.line << " in " << info.dso;
}

} // namespace lo2s
