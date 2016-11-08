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
#ifndef LO2S_SYSTEM_TREE_HPP
#define LO2S_SYSTEM_TREE_HPP

#include <boost/filesystem.hpp>

namespace lo2s {

class package
{
    int id;
};


class core
{
    int id;
    package
};
class cpu
{

};


class topology
{
    topology() {

    }
    std::vector<cpu> cpus;
    std::vector<core> cores;
    std::vector<packag> packages;
};
}

#endif //LO2S_SYSTEM_TREE_HPP
