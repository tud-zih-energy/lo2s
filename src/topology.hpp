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
#ifndef INCLUDE_ROCO2_CPU_TOPOLOGY_HPP
#define INCLUDE_ROCO2_CPU_TOPOLOGY_HPP

#include <cassert>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// TODO replace asserts with execptions

#include <boost/filesystem.hpp>
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
}

class topology
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

            auto insert_package = packages_.insert(std::make_pair(package_id, package(package_id)));
            auto& package = insert_package.first->second;
            assert(package.id == package_id);
            package.core_ids.insert(core_id);
            package.cpu_ids.insert(cpu_id);

            auto insert_core = cores_.insert(
                std::make_pair(std::make_tuple(package_id, core_id), core(core_id, package_id)));
            auto& core = insert_core.first->second;
            assert(core.id == core_id && core.package_id == package_id);
            core.cpu_ids.insert(cpu_id);

            auto insert_cpu =
                cpus_.insert(std::make_pair(cpu_id, cpu(cpu_id, core_id, package_id)));
            auto& cpu = insert_cpu.first->second;
            (void)cpu;
            assert(cpu.id == cpu_id && cpu.core_id == core_id && cpu.package_id == package_id);
            if (!insert_cpu.second)
            {
                throw std::runtime_error("Duplicate cpu_id when reading the topology.");
            }
        }
    }

public:
    struct cpu
    {
        uint32_t id;
        uint32_t core_id;
        uint32_t package_id;

        cpu(uint32_t id, uint32_t core_id, uint32_t package_id)
        : id(id), core_id(core_id), package_id(package_id)
        {
        }
    };

    struct core
    {
        uint32_t id;
        uint32_t package_id;

        core(uint32_t id, uint32_t package_id) : id(id), package_id(package_id)
        {
        }

        std::set<uint32_t> cpu_ids;
    };

    struct package
    {
        uint32_t id;

        package(uint32_t id) : id(id)
        {
        }

        std::set<uint32_t> core_ids;
        std::set<uint32_t> cpu_ids;
    };

private:
    topology()
    {
        read_proc();
    }

public:
    static topology& instance()
    {
        static topology t;

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

    std::vector<cpu> cpus() const
    {
        std::vector<cpu> r;
        std::transform(cpus_.begin(), cpus_.end(), std::back_inserter(r),
                       [](const auto& elem) { return elem.second; });
        return r;
    }
    std::vector<core> cores() const
    {
        std::vector<core> r;
        std::transform(cores_.begin(), cores_.end(), std::back_inserter(r),
                       [](const auto& elem) { return elem.second; });
        return r;
    }
    std::vector<package> packages() const
    {
        std::vector<package> r;
        std::transform(packages_.begin(), packages_.end(), std::back_inserter(r),
                       [](const auto& elem) { return elem.second; });
        return r;
    }

private:
    std::map<int, cpu> cpus_;
    std::map<std::tuple<int, int>, core> cores_;
    std::map<int, package> packages_;

    const static fs::path base_path;
};

}

#endif // INCLUDE_ROCO2_CPU_TOPOLOGY_HPP
