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
#ifndef LO2S_LINE_INFO_HPP
#define LO2S_LINE_INFO_HPP

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

using fmt = boost::format;

namespace lo2s
{
struct line_info
{
    // Note: If line is not known, we write 1 anyway so the rest is shown in vampir
    line_info(const std::string& fi, const std::string& fu, unsigned int l,
              const std::string& d)
            : file(fi), function(fu), line(l ? l : 1),
              dso(boost::filesystem::path(d).filename().string())
    {
    }

    line_info(const char* fi, const char* fu, unsigned int l, const std::string& d)
            : line_info(fi ? std::string(fi) : unknown_str, fu ? std::string(fu) : unknown_str, l, d)
    {
    }

    line_info(address addr) : line_info(addr, unknown_str)
    {
    }

    line_info(address addr, const std::string& d)
            : line_info(unknown_str, str(fmt("?@%dx") % addr), 0, d)
    {
    }

    std::string file;
    std::string function;
    unsigned int line;
    std::string dso;

    static const std::string unknown_str;

    // For std::map
    bool operator<(const line_info& other) const
    {
        return std::tie(file, function, line, dso) <
               std::tie(other.file, other.function, other.line, dso);
    }
    bool operator==(const line_info& other) const
    {
        return std::tie(file, function, line, dso) ==
               std::tie(other.file, other.function, other.line, dso);
    }
};

inline std::ostream& operator<<(std::ostream& os, const line_info& info)
{
    return os << info.function << " @ " << info.file << ":" << info.line << " in " << info.dso;
}

}

#endif //LO2S_LINE_INFO_HPP
