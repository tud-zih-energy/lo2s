/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2019,
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
#include <lo2s/ipc/fifo.hpp>
#include <lo2s/line_info.hpp>
#include <lo2s/log.hpp>

#include <map>
#include <string>
#include <vector>

namespace lo2s
{
namespace java
{

class JVMSymbols
{
public:
    static std::unique_ptr<JVMSymbols> instance;

    JVMSymbols(pid_t jvm_pid);

    LineInfo lookup(Address addr) const;
    void read_symbols();

private:
    void attach();

    pid_t pid_;
    ipc::Fifo fifo_;
    std::map<Range, std::string> symbols_;
};
} // namespace java
} // namespace lo2s
