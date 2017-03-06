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

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

using fmt = boost::format;

namespace lo2s
{
struct LineInfo
{
    // Note: If line is not known, we write 1 anyway so the rest is shown in vampir
    LineInfo(const std::string& fi, const std::string& fu, unsigned int l,
              const std::string& d)
            : file(fi), function(fu), line(l ? l : 1),
              dso(boost::filesystem::path(d).filename().string())
    {
    }

    LineInfo(const char* fi, const char* fu, unsigned int l, const std::string& d)
            : LineInfo(fi ? std::string(fi) : unknown_str, fu ? std::string(fu) : unknown_str, l, d)
    {
    }

    LineInfo(Address addr) : LineInfo(addr, unknown_str)
    {
    }

    LineInfo(Address addr, const std::string& d)
            : LineInfo(unknown_str, str(fmt("?@%dx") % addr), 0, d)
    {
    }

    std::string file;
    std::string function;
    unsigned int line;
    std::string dso;

    static const std::string unknown_str;

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

}
