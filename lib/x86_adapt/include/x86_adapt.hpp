/*
 * Copyright (c) 2016, Technische Universität Dresden, Germany
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
}
