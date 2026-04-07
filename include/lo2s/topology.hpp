// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/types/core.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/types/package.hpp>

#include <algorithm>
#include <map>
#include <set>

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

private:
    std::set<Cpu> cpus_;
    std::set<Package> packages_;
    std::map<Cpu, Core> cpu_to_core_;
    std::map<Cpu, Package> cpu_to_package_;

    bool hypervised_ = false;
};
} // namespace lo2s
