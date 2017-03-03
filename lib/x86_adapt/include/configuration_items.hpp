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

#include <string>

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

        bool operator!=(const iterator& other)
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
}
