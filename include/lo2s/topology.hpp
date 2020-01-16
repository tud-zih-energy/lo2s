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
#include <tuple>
#include <utility>
#include <vector>

#include <cstdint>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>

using fmt = boost::format;
namespace fs = boost::filesystem;
using namespace std::literals::string_literals;

namespace lo2s
{
class Topology
{

    void read_proc();

public:
    struct Cpu
    {
        uint32_t id;
        uint32_t core_id;
        uint32_t package_id;

        Cpu(uint32_t id, uint32_t core_id, uint32_t package_id)
        : id(id), core_id(core_id), package_id(package_id)
        {
        }
    };

    struct Core
    {
        uint32_t id;
        uint32_t package_id;

        Core(uint32_t id, uint32_t package_id) : id(id), package_id(package_id)
        {
        }

        std::set<uint32_t> cpu_ids;
    };

    struct Package
    {
        uint32_t id;

        Package(uint32_t id) : id(id)
        {
        }

        std::set<uint32_t> core_ids;
        std::set<uint32_t> cpu_ids;
    };

private:
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

    std::size_t num_cores() const
    {
        return cores_.size();
    }

    std::size_t num_packages() const
    {
        return packages_.size();
    }

    std::vector<Cpu> cpus() const
    {
        std::vector<Cpu> r;
        std::transform(cpus_.begin(), cpus_.end(), std::back_inserter(r),
                       [](const auto& elem) { return elem.second; });
        return r;
    }
    std::vector<Core> cores() const
    {
        std::vector<Core> r;
        std::transform(cores_.begin(), cores_.end(), std::back_inserter(r),
                       [](const auto& elem) { return elem.second; });
        return r;
    }
    std::vector<Package> packages() const
    {
        std::vector<Package> r;
        std::transform(packages_.begin(), packages_.end(), std::back_inserter(r),
                       [](const auto& elem) { return elem.second; });
        return r;
    }

private:
    std::map<int, Cpu> cpus_;
    std::map<std::tuple<int, int>, Core> cores_;
    std::map<int, Package> packages_;

    bool hypervised_ = false;

    const static fs::path base_path;
};
} // namespace lo2s
