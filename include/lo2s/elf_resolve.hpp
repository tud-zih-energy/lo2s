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
#include <lo2s/line_info.hpp>
#include <lo2s/log.hpp>

#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <vector>

#include <cassert>

namespace lo2s
{
namespace elfr
{
class Elf
{
public:
    Elf(const std::string& name);
    Elf(const Elf&) = delete;
    Elf(Elf&&) = delete;
    Elf& operator=(const Elf&) = delete;
    Elf& operator=(Elf&&) = delete;

    LineInfo lookup(Address addr) const;

    const std::string& name() const
    {
        return name_;
    }

private:
    std::string name_;
    std::map<Range, LineInfo> symbols_;
};
} // namespace elfr
} // namespace lo2s
