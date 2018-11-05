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

namespace detail
{

inline auto parse_list(std::string list) -> std::set<uint32_t>
{
    std::stringstream s;
    s << list;

    std::set<uint32_t> res;

    std::string part;
    while (std::getline(s, part, ','))
    {
        auto pos = part.find('-');
        if (pos != std::string::npos)
        {
            // is a range
            uint32_t from = std::stoi(part.substr(0, pos));
            uint32_t to = std::stoi(part.substr(pos + 1));

            for (auto i = from; i <= to; ++i)
                res.insert(i);
        }
        else
        {
            // single value
            res.insert(std::stoi(part));
        }
    }

    return res;
}
} // namespace detail

class Topology
{

    void read_proc()
    {
        std::string online_list;
        std::string present_list;

        {
            fs::ifstream cpu_online(base_path / "/online");
            std::getline(cpu_online, online_list);

            fs::ifstream cpu_present(base_path / "/present");
            std::getline(cpu_present, present_list);
        }

        auto online = detail::parse_list(online_list);
        auto present = detail::parse_list(present_list);

        for (auto cpu_id : online)
        {
            std::stringstream filename_stream;

            fs::path topology = base_path / ("cpu"s + std::to_string(cpu_id)) / "topology";
            fs::ifstream package_stream(topology / "physical_package_id");
            fs::ifstream core_stream(topology / "core_id");

            uint32_t package_id, core_id;
            package_stream >> package_id;
            core_stream >> core_id;

            auto insert_package = packages_.insert(std::make_pair(package_id, Package(package_id)));
            auto& package = insert_package.first->second;
            if (package.id != package_id)
            {
                throw std::runtime_error("Inconsistent package ids in topology.");
            }
            package.core_ids.insert(core_id);
            package.cpu_ids.insert(cpu_id);

            auto insert_core = cores_.insert(
                std::make_pair(std::make_tuple(package_id, core_id), Core(core_id, package_id)));
            auto& core = insert_core.first->second;
            if (core.id != core_id || core.package_id != package_id)
            {
                throw std::runtime_error("Inconsistent package/core ids in topology.");
            }
            core.cpu_ids.insert(cpu_id);

            auto insert_cpu =
                cpus_.insert(std::make_pair(cpu_id, Cpu(cpu_id, core_id, package_id)));
            auto& cpu = insert_cpu.first->second;
            (void)cpu;
            if (cpu.id != cpu_id || cpu.core_id != core_id || cpu.package_id != package_id)
            {
                throw std::runtime_error("Inconsistent cpu/package/core ids in topology.");
            }
            if (!insert_cpu.second)
            {
                throw std::runtime_error("Duplicate cpu_id when reading the topology.");
            }
        }
    }

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

    const static fs::path base_path;
};
} // namespace lo2s
