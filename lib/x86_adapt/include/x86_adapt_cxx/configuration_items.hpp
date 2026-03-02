// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <x86_adapt_cxx/configuration_item.hpp>
#include <x86_adapt_cxx/exception.hpp>

#include <string>

#include <x86_adapt.h>

namespace x86_adapt
{

class configuration_items
{
public:
    configuration_items(x86_adapt_device_type type)
    : type_(type), num_items_(check(x86_adapt_get_number_cis(type)))
    {
    }

    int size() const
    {
        return num_items_;
    }

    configuration_item lookup(const std::string& knob)
    {
        return configuration_item(type_, knob);
    }

    configuration_item operator()(const std::string& knob)
    {
        return lookup(knob);
    }

    struct iterator
    {
        iterator(x86_adapt_device_type type, int id) : id_(id), type_(type)
        {
        }

        iterator& operator++()
        {
            id_++;
            return *this;
        }

        iterator operator++(int)
        {
            iterator result(*this);
            ++(*this);
            return result;
        }

        configuration_item operator*()
        {
            return configuration_item(type_, id_);
        }

        bool operator!=(const iterator& other) const
        {
            return id_ != other.id_;
        }

    private:
        int id_;
        x86_adapt_device_type type_;
    };

    iterator begin() const
    {
        return iterator(type_, 0);
    }

    iterator end() const
    {
        return iterator(type_, num_items_);
    }

private:
    x86_adapt_device_type type_;
    int num_items_;
};
} // namespace x86_adapt
