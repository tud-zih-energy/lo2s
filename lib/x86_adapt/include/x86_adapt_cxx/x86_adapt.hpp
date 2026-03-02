// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <x86_adapt_cxx/configuration_items.hpp>
#include <x86_adapt_cxx/device.hpp>
#include <x86_adapt_cxx/exception.hpp>

#include <x86_adapt.h>

namespace x86_adapt
{

class x86_adapt
{
public:
    x86_adapt()
    {
        check(x86_adapt_init());
    }

    x86_adapt(const x86_adapt&) = delete;
    x86_adapt& operator=(const x86_adapt&) = delete;

    x86_adapt(x86_adapt&&) = delete;
    x86_adapt& operator=(x86_adapt&&) = delete;

    ~x86_adapt()
    {
        x86_adapt_finalize();
    }

public:
    int num_nodes()
    {
        return check(x86_adapt_get_nr_avaible_devices(X86_ADAPT_DIE));
    }

    int num_cpus()
    {
        return check(x86_adapt_get_nr_avaible_devices(X86_ADAPT_CPU));
    }

    device node(int id)
    {
        return device(X86_ADAPT_DIE, id);
    }

    device cpu(int id)
    {
        return device(X86_ADAPT_CPU, id);
    }

public:
    configuration_items cpu_configuration_items()
    {
        return configuration_items(X86_ADAPT_CPU);
    }

    configuration_items node_configuration_items()
    {
        return configuration_items(X86_ADAPT_DIE);
    }
};
} // namespace x86_adapt
