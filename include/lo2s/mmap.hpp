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

#pragma once

#include <lo2s/address.hpp>
#include <lo2s/bfd_resolve.hpp>
#include <lo2s/radare.hpp>
#include <lo2s/util.hpp>

#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace lo2s
{

class binary
{
protected:
    binary(const std::string& name) : name_(name)
    {
    }

public:
    virtual ~binary()
    {
    }

    virtual std::string lookup_instruction(address ip) = 0;
    virtual line_info lookup_line_info(address ip) = 0;
    const std::string& name() const
    {
        return name_;
    };

private:
    std::string name_;
};

class named_binary : public binary
{
public:
    named_binary(const std::string& name) : binary(name)
    {
    }

    static binary& cache(const std::string& name)
    {
        return string_cache<named_binary>::instance()[name];
    }

    virtual std::string lookup_instruction(address) override
    {
        throw std::domain_error("Unknown instruction.");
    }

    virtual line_info lookup_line_info(address) override
    {
        return line_info(name(), name(), 0, name());
    }
};

class bfd_radare_binary : public binary
{
public:
    bfd_radare_binary(const std::string& name) : binary(name), bfd_(name), radare_(name)
    {
    }

    static binary& cache(const std::string& name)
    {
        return string_cache<bfd_radare_binary>::instance()[name];
    }

    virtual std::string lookup_instruction(address ip) override
    {
        return radare_.instruction(ip);
    }

    virtual line_info lookup_line_info(address ip) override
    {
        try
        {
            return bfd_.lookup(ip);
        }
        catch (bfdr::lookup_error&)
        {
            return line_info(ip, name());
        }
    }

private:
    bfdr::lib bfd_;
    radare_resolver radare_;
};

class memory_map
{
public:
    memory_map();
    memory_map(pid_t pid, bool read_initial);

    void mmap(address begin, address end, address pgoff, const std::string& dso_name);

    line_info lookup_line_info(address ip) const;
    // Will throw alot - catch it if you can
    std::string lookup_instruction(address ip) const;

private:
    struct mapping
    {
        mapping(address s, address e, address o, binary& d) : start(s), end(e), pgoff(o), dso(d)
        {
        }

        address start;
        address end;
        address pgoff;
        binary& dso;
    };

    std::map<range, mapping> map_;
};
}
