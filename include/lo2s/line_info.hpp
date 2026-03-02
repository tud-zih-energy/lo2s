// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>

namespace lo2s
{
struct LineInfo
{
private:
    static constexpr unsigned int UNKNOWN_LINE = 0;

    // Note: If line is not known, we write 1 anyway so the rest is shown in vampir
    // phijor 2018-11-08: I think this workaround is not needed anymore? vampir
    // shows source code locations with line == 0 just fine.
    LineInfo(std::string file, std::string function, unsigned int line, std::string dso)
    : file(std::move(file)), function(std::move(function)), line((line != UNKNOWN_LINE) ? line : 1),
      dso(std::move(dso))
    {
    }

    LineInfo(const char* file, const char* function, unsigned int line, std::string dso)
    : file(file), function(function), line((line != UNKNOWN_LINE) ? line : 1), dso(std::move(dso))
    {
    }

public:
    static LineInfo for_function(const char* file, const char* function, unsigned int line,
                                 const std::string& dso)
    {
        return { (file != nullptr) ? file : "<unknown file>",
                 (function != nullptr) ? function : "<unknown function>", line,
                 std::filesystem::path(dso).filename().string() };
    }

    static LineInfo for_unknown_function()
    {
        return { "<unknown file>", "<unknown function>", UNKNOWN_LINE, "<unknown binary>" };
    }

    static LineInfo for_unknown_function_in_dso(const std::string& dso)
    {
        return { "<unknown file>", "<unknown function>", UNKNOWN_LINE,
                 std::filesystem::path(dso).filename().string() };
    }

    static LineInfo for_binary(const std::string& binary)
    {
        return { binary, binary, UNKNOWN_LINE, binary };
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
