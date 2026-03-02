// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    uint64_t get_setting(const configuration_item& item)
    {
        uint64_t result;

        if (type_ != item.type())
        {
            // TODO throw exceptions like oger throw rocks: big and hard
        }

        check(x86_adapt_get_setting(handle_, item.id(), &result));

        return result;
    }

    uint64_t operator()(const configuration_item& item)
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
} // namespace x86_adapt
