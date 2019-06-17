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
#include <lo2s/line_info.hpp>
#include <lo2s/log.hpp>

#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
using fmt = boost::format;
using boost::str;
using std::hex;

#include <cassert>

extern "C"
{
// https://sourceware.org/bugzilla/show_bug.cgi?id=14243
#define PACKAGE 1
#define PACKAGE_VERSION 1
#include <bfd.h>
// A really cool API hides it's magic constants!
#ifndef DMGL_PARAMS
#define DMGL_NO_OPTS 0       /* For readability... */
#define DMGL_PARAMS (1 << 0) /* Include function args */
#define DMGL_ANSI (1 << 1)   /* Include const, volatile, etc */
#endif

#include <unistd.h>
}

namespace lo2s
{
namespace bfdr
{

struct Initializer
{
    Initializer()
    {
        bfd_init();
    }
};

class InitError : public std::runtime_error
{
public:
    InitError(const std::string& what, const std::string& lib)
    : std::runtime_error(what + ": " + lib)
    {
    }

    InitError(const std::string& what) : std::runtime_error(what)
    {
    }
};

class InvalidFileError : public std::runtime_error
{
public:
    InvalidFileError(const std::string& what, const std::string& dso)
    : std::runtime_error(what + ": " + dso)
    {
    }
};

class LookupError : public std::runtime_error
{
public:
    LookupError(const std::string& what, Address addr) : std::runtime_error(msg(what, addr))
    {
    }

private:
    static std::string msg(const std::string& what, Address addr)
    {
        std::stringstream ss;
        ss << what << ": 0x" << hex << addr;
        return ss.str();
    }
};

struct bfd_handle_deleter
{
    void operator()(bfd* p) const
    {
        bfd_close(p);
    }
};

struct char_handle_deleter
{
    void operator()(char* p) const
    {
        free(p);
    }
};

using unique_bfd_ptr = std::unique_ptr<bfd, bfd_handle_deleter>;
using unique_char_ptr = std::unique_ptr<char, char_handle_deleter>;

class Lib
{
public:
    Lib(const std::string& name);
    Lib(const Lib&) = delete;
    Lib(Lib&&) = delete;
    Lib& operator=(const Lib&) = delete;
    Lib& operator=(Lib&&) = delete;

    LineInfo lookup(Address addr) const;

    const std::string& name() const
    {
        return name_;
    }

private:
    void filter_symbols()
    {
        symbols_.erase(std::remove_if(symbols_.begin(), symbols_.end(), [](const asymbol* sym) {
            return (sym != nullptr) && !(sym->flags & BSF_FUNCTION);
        }));
    }

    void read_symbols();

    void read_sections();

    template <typename T>
    static T check_symtab(T sz)
    {
        if (sz < 0)
        {
            throw std::runtime_error("Error getting symtab size");
        }
        return sz;
    }

    std::string name_;
    unique_bfd_ptr handle_;
    std::vector<asymbol*> symbols_;
    std::map<Range, asection*> sections_;

    static Initializer dummy_;
};
} // namespace bfdr
} // namespace lo2s
