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

#include <x86_adapt_cxx/exception.hpp>

#include <x86_adapt.h>

#include <string>

namespace x86_adapt
{

class configuration_item
{
public:
    configuration_item(x86_adapt_device_type type, const std::string& name)
    : type_(type), handle_(check(x86_adapt_lookup_ci_name(type, name.c_str())))
    {
    }

    configuration_item(x86_adapt_device_type type, int id) : type_(type), handle_(id)
    {
    }

    std::string name() const
    {
        x86_adapt_configuration_item item;

        check(x86_adapt_get_ci_definition(type_, handle_, &item));

        return item.name;
    }

    std::string description() const
    {
        x86_adapt_configuration_item item;

        check(x86_adapt_get_ci_definition(type_, handle_, &item));

        return item.description;
    }

    x86_adapt_device_type type() const
    {
        return type_;
    }

    int id() const
    {
        return handle_;
    }

    bool operator<(const configuration_item& other)
    {
        return handle_ < other.handle_;
    }

private:
    x86_adapt_device_type type_;
    int handle_;
};

inline bool operator<(const configuration_item& a, const configuration_item& b)
{
    return a.id() < b.id();
}
}
