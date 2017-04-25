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
#ifdef HAVE_RADARE
#include <lo2s/radare.hpp>
#endif
#include <lo2s/util.hpp>

#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace lo2s
{

class Binary
{
protected:
    Binary(const std::string& name) : name_(name)
    {
    }

public:
    virtual ~Binary()
    {
    }

    virtual std::string lookup_instruction(Address)
    {
        throw std::runtime_error("lookup_instruction not implemented.");
    };
    virtual LineInfo lookup_line_info(Address ip) = 0;
    const std::string& name() const
    {
        return name_;
    };

private:
    std::string name_;
};

class NamedBinary : public Binary
{
public:
    NamedBinary(const std::string& name) : Binary(name)
    {
    }

    static Binary& cache(const std::string& name)
    {
        return StringCache<NamedBinary>::instance()[name];
    }

    virtual std::string lookup_instruction(Address) override
    {
        throw std::domain_error("Unknown instruction.");
    }

    virtual LineInfo lookup_line_info(Address) override
    {
        return LineInfo(name(), name(), 0, name());
    }
};

/**
 * TODO Split this... it's ugly
 */
class BfdRadareBinary : public Binary
{
public:
    BfdRadareBinary(const std::string& name)
    : Binary(name), bfd_(name)
#ifdef HAVE_RADARE
      ,
      radare_(name)
#endif
    {
    }

    static Binary& cache(const std::string& name)
    {
        return StringCache<BfdRadareBinary>::instance()[name];
    }

#ifdef HAVE_RADARE
    virtual std::string lookup_instruction(Address ip) override
    {
        return radare_.instruction(ip);
    }
#endif

    virtual LineInfo lookup_line_info(Address ip) override
    {
        try
        {
            return bfd_.lookup(ip);
        }
        catch (bfdr::LookupError&)
        {
            return LineInfo(ip, name());
        }
    }

private:
    bfdr::Lib bfd_;
#ifdef HAVE_RADARE
    RadareResolver radare_;
#endif // HAVE_RADARE
};

class MemoryMap
{
public:
    MemoryMap();
    MemoryMap(pid_t pid, bool read_initial);

    void mmap(Address begin, Address end, Address pgoff, const std::string& dso_name);

    LineInfo lookup_line_info(Address ip) const;

    // Will throw alot - catch it if you can
    std::string lookup_instruction(Address ip) const;

private:
    struct Mapping
    {
        Mapping(Address s, Address e, Address o, Binary& d) : start(s), end(e), pgoff(o), dso(d)
        {
        }

        Address start;
        Address end;
        Address pgoff;
        Binary& dso;
    };

    std::map<Range, Mapping> map_;
};
}
