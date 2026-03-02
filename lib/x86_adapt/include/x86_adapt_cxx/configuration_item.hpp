// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <x86_adapt_cxx/exception.hpp>

#include <string>

#include <x86_adapt.h>

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
} // namespace x86_adapt
