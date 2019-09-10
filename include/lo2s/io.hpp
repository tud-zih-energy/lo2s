/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

/**
 * \file lo2s/io.hpp
 * \brief I/O helper functions for lo2s' types
 *
 * This header contains overloads for stream-based formatted output of types
 * used by lo2s and I/O-manipulators for uniform looking output.
 *
 */

#pragma once

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

#include <boost/algorithm/string.hpp>
#include <lo2s/time/time.hpp>

namespace lo2s
{

namespace time
{
inline std::ostream& operator<<(std::ostream& os, const ClockDescription& desc)
{
    return os << desc.name;
}
} // namespace time

namespace io
{

/**
 * \brief Format a range of values as a list of arguments to an option.
 */
template <class InputIterator>
struct ArgumentList
{
    ArgumentList() = delete;

    /**
     * \param description   description of list contents printed before list
     * \param first         iterator pointing to first item to print
     * \param last          iterator pointing past the last item to print
     * */
    constexpr ArgumentList(const std::string& description, InputIterator first, InputIterator last)
    : description_(description), first_(first), last_(last)
    {
    }

    template <class _InputIt>
    friend ArgumentList<InputIterator> make_argument_list(const std::string&, _InputIt, _InputIt);

    template <class _InputIt>
    friend std::ostream& operator<<(std::ostream&, const ArgumentList<_InputIt>&);

private:
    std::string description_;
    InputIterator first_, last_;
};

/**
 * \brief   Creates ArgumentList object, deducing the target type from the types
 *          of the arguments
 */
template <class InputIterator>
inline constexpr auto make_argument_list(const std::string& description, InputIterator first,
                                         InputIterator last)
{
    return ArgumentList<InputIterator>{ description, first, last };
}

template <class InputIterator>
inline std::ostream& operator<<(std::ostream& os, const ArgumentList<InputIterator>& list)
{
    os << "\nList of " << list.description_ << ":\n\n";

    if (list.first_ != list.last_)
    {
        for (auto it = list.first_; it != list.last_; ++it)
        {
            os << "  " << *it << '\n';
        }
    }
    else
    {
        os << "  (none available)\n";
    }
    os << '\n';
    return os;
}

#ifdef HAVE_X86_ADAPT
template <>
inline std::ostream&
operator<<(std::ostream& os, const ArgumentList<std::map<std::string, std::string>::iterator>& list)
{
    os << "\nList of " << list.description_ << ":\n\n";
    if (list.first_ != list.last_)
    {
        size_t max_string_length = 0;
        for (auto it = list.first_; it != list.last_; ++it)
        {
            max_string_length = std::max(max_string_length, it->first.length());
        }
        for (auto it = list.first_; it != list.last_; ++it)
        {
            std::vector<std::string> lines;
            boost::split(lines, it->second, [](char c) { return c == '\n'; });
            os << "  " << std::left << std::setw(max_string_length + 2) << it->first << lines[0]
               << '\n';
            lines.erase(lines.begin());
            for (const auto& line : lines)
            {
                os << "  " << std::left << std::setw(max_string_length + 2) << "" << line << "\n";
            }
        }
    }
    else
    {
        os << "  (none available)\n";
    }
    os << '\n';
    return os;
}
#endif
} // namespace io
} // namespace lo2s
