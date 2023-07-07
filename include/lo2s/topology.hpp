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

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <cstdint>

#include <filesystem>

#include <lo2s/types.hpp>

using namespace std::literals::string_literals;

namespace lo2s
{
class Topology
{

private:
    void read_proc();

    Topology()
    {
        read_proc();
    }

public:
    static Topology& instance()
    {
        static Topology t;

        return t;
    }

    bool hypervised() const
    {
        return hypervised_;
    }

    const std::set<Cpu>& cpus() const
    {
        return cpus_;
    }

    Core core_of(Cpu cpu) const
    {
        return cpu_to_core_.at(cpu);
    }

    const std::set<Package>& packages() const
    {
        return packages_;
    }

    Package package_of(Cpu cpu) const
    {
        return cpu_to_package_.at(cpu);
    }

    Cpu measuring_cpu_for_package(Package package) const
    {
        auto package_it = std::find_if(cpu_to_package_.begin(), cpu_to_package_.end(),
                                       [package](auto elem) { return elem.second == package; });

        if (package_it != cpu_to_package_.end())
        {
            return package_it->first;
        }
        return Cpu::invalid();
    }

    const std::set<Gpu>& gpus() const
    {
        return gpus_;
    }

private:
    std::set<Cpu> cpus_;
    std::set<Package> packages_;
    std::map<Cpu, Core> cpu_to_core_;
    std::map<Cpu, Package> cpu_to_package_;
    std::set<Gpu> gpus_;

    bool hypervised_ = false;

    const static std::filesystem::path base_path;
};
} // namespace lo2s
