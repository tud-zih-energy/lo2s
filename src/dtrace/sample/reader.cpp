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

#include <lo2s/dtrace/sample/reader.hpp>

#include <sstream>

extern "C"
{
#include <dtrace.h>
}

namespace lo2s
{
namespace dtrace
{
namespace sample
{

void Reader::init(pid_t tid, int cpu)
{
    std::stringstream str;

    str << char* script2 = "sched:::on-cpu { printf(\"%d %d %d\", walltimestamp, cpu, pid); } "
                           "sched:::off-cpu { printf(\"%d %d %d\", walltimestamp, cpu, pid); }";

    auto prog = "";
}

void Reader::handle(int cpu, const ProbeDesc& pd, nitro::lang::string_ref data)
{
}

} // namespace sample
} // namespace dtrace
} // namespace lo2s
