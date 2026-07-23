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
struct FunctionInfo
{
    std::string file;
    std::string function;
    unsigned int begin_line;
    unsigned int end_line;
    std::string dso;

    bool operator<(const FunctionInfo& other) const
    {
        return std::tie(file, function, begin_line, end_line, dso) <
               std::tie(other.file, other.function, other.begin_line, other.end_line, other.dso);
    }

    bool operator==(const FunctionInfo& other) const
    {
        return std::tie(file, function, begin_line, end_line, dso) ==
               std::tie(other.file, other.function, other.begin_line, other.end_line, other.dso);
    }
};

struct LineInfo
{
private:
    static constexpr unsigned int UNKNOWN_LINE = 0;

    // Note: If line is not known, we write 1 anyway so the rest is shown in vampir
    // phijor 2018-11-08: I think this workaround is not needed anymore? vampir
    // shows source code locations with line == 0 just fine.
    static unsigned int normalize_line(unsigned int line)
    {
        return (line != UNKNOWN_LINE) ? line : 1;
    }

    LineInfo(std::string file, std::string function, unsigned int line, std::string dso,
             unsigned int function_begin_line = UNKNOWN_LINE,
             unsigned int function_end_line = UNKNOWN_LINE, std::string function_file = "")
    : file(std::move(file)), function(std::move(function)), line((line != UNKNOWN_LINE) ? line : 1),
      dso(std::move(dso)), function_begin_line(normalize_line(function_begin_line)),
      function_end_line(function_end_line), function_file(std::move(function_file))
    {
    }

    LineInfo(const char* file, const char* function, unsigned int line, std::string dso,
             unsigned int function_begin_line = UNKNOWN_LINE,
             unsigned int function_end_line = UNKNOWN_LINE, std::string function_file = "")
    : file(file), function(function), line((line != UNKNOWN_LINE) ? line : 1),
      dso(std::move(dso)), function_begin_line(normalize_line(function_begin_line)),
      function_end_line(function_end_line), function_file(std::move(function_file))
    {
    }

public:
    static LineInfo for_function(const char* file, const char* function, unsigned int line,
                                 const std::string& dso,
                                 unsigned int function_begin_line = UNKNOWN_LINE,
                                 unsigned int function_end_line = UNKNOWN_LINE,
                                 const char* function_file = nullptr)
    {
        return { (file != nullptr) ? file : "<unknown file>",
                 (function != nullptr) ? function : "<unknown function>", line,
                 std::filesystem::path(dso).filename().string(), function_begin_line,
                 function_end_line, (function_file != nullptr) ? function_file : "" };
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
    unsigned int function_begin_line;
    unsigned int function_end_line;
    std::string function_file;

    FunctionInfo function_info() const
    {
        return { function_file.empty() ? file : function_file, function, function_begin_line,
                 function_end_line, dso };
    }

    // For std::map
    bool operator<(const LineInfo& other) const
    {
        return std::tie(file, function, line, dso, function_begin_line, function_end_line,
                        function_file) <
               std::tie(other.file, other.function, other.line, other.dso,
                        other.function_begin_line, other.function_end_line, other.function_file);
    }

    bool operator==(const LineInfo& other) const
    {
        return std::tie(file, function, line, dso, function_begin_line, function_end_line,
                        function_file) ==
               std::tie(other.file, other.function, other.line, other.dso,
                        other.function_begin_line, other.function_end_line, other.function_file);
    }
};

inline std::ostream& operator<<(std::ostream& os, const LineInfo& info)
{
    return os << info.function << " @ " << info.file << ":" << info.line << " in " << info.dso;
}

} // namespace lo2s
