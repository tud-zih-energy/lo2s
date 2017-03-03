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

#include <x86_adapt_cxx/configuration_item.hpp>
#include <x86_adapt_cxx/exception.hpp>

#include <x86_adapt.h>

namespace x86_adapt
{

class device
{
public:
    device(x86_adapt_device_type type, int dev)
    : type_(type), handle_(check(x86_adapt_get_device_ro(type, dev))), dev_(dev)
    {
    }

    device(const device&) = delete;
    device& operator=(const device&) = delete;

    device(device&& other) : type_(other.type_), handle_(other.handle_), dev_(other.dev_)
    {
        other.dev_ = -1;
    }

    device& operator=(device&& other) = delete;

    ~device()
    {
        if (dev_ != -1)
        {
            check(x86_adapt_put_device(type_, dev_));
        }
    }

public:
    std::uint64_t get_setting(const configuration_item& item)
    {
        std::uint64_t result;

        if (type_ != item.type())
        {
            // TODO throw exceptions like oger throw rocks: big and hard
        }

        check(x86_adapt_get_setting(handle_, item.id(), &result));

        return result;
    }

    std::uint64_t operator()(const configuration_item& item)
    {
        return get_setting(item);
    }

    int id() const
    {
        return dev_;
    }

    int handle() const
    {
        return handle_;
    }

    x86_adapt_device_type type() const
    {
        return type_;
    }

private:
    x86_adapt_device_type type_;
    int handle_;
    int dev_;
};
}
