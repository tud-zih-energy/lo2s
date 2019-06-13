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
#include <lo2s/address.hpp>
#include <lo2s/bfd_resolve.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>

#include <cstdlib>

namespace lo2s
{
namespace bfdr
{

static boost::filesystem::path check_path(const std::string& name)
{
    boost::system::error_code ec;
    auto path = boost::filesystem::canonical(name, ec);
    if (ec)
    {
        throw InvalidFileError("could not resolve to canonical path", name);
    }

    auto status = boost::filesystem::status(path, ec);
    if (ec)
    {
        throw InvalidFileError("could not read status", name);
    }

    if (!boost::filesystem::is_regular_file(status))
    {
        throw InvalidFileError("not a regular file", name);
    }

    return path;
}

Initializer Lib::dummy_;

Lib::Lib(const std::string& name)
: name_(name), handle_(bfd_openr(check_path(name).c_str(), nullptr))
{
    if (!handle_)
    {
        throw InitError("failed to open BFD handle", name);
    }
    // init stuff
    //  Not sure if those are needed for side-effects
    bfd_get_arch_size(handle_.get());
    bfd_find_target(nullptr, handle_.get());
    if (!bfd_check_format(handle_.get(), bfd_object))
    {
        throw InitError("Not a valid bfd_object", name);
    }

    char** matching;
    if (bfd_check_format_matches(handle_.get(), bfd_archive, &matching))
    {
        throw InitError("BFD format does not match.", name);
    }

    //            if (bfd_get_flavour(handle_) != bfd_target_elf_flavour)
    //            {
    //                log::error() << "Only supporting elf right now. Need symbol size.";
    //                throw std::runtime_error("BFD flavor not ELF.");
    //            }

    read_symbols();
    read_sections();
}

LineInfo Lib::lookup(Address addr) const
{
    const char* file = nullptr;
    const char* func = nullptr;
    unsigned int line = 0;

    try
    {
        auto section = sections_.at(addr);
        // VMA is useless for shared libraries, we remove the offset from the map
        // so use filepos instead
        // auto base = bfd_get_section_vma(handle_, section);
        auto base = section->filepos;
        auto size = bfd_get_section_size(section);
        auto local_addr = addr - base;
        if (local_addr >= base + size)
        {
            // This should not happen as long as we lookup sections in the range-map by
            // address, since base/size is the same as used for the ranges
            Log::error() << "address " << addr << " out of section bounds " << name_ << " "
                         << bfd_get_section_name(handle_.get(), section) << " filepos: " << hex
                         << base << ", size: " << hex << size;
            throw LookupError("address out of .symtab bounds", addr);
        }
        auto evil_symtab = const_cast<asymbol**>(symbols_.data());
        auto found = bfd_find_nearest_line(handle_.get(), section, evil_symtab, local_addr.value(),
                                           &file, &func, &line);
        if (!found)
        {
            Log::debug() << "bfd_find_nearest_line failed " << addr << " in " << name_
                         << bfd_get_section_name(handle_.get(), section);
            throw LookupError("bfd_find_nearest_line failed", addr);
        }
        if (func != nullptr)
        {
            unique_char_ptr demangled(bfd_demangle(handle_.get(), func, DMGL_PARAMS | DMGL_ANSI));
            if (!demangled)
            {
                LineInfo line_info = LineInfo::for_function(file, demangled.get(), line, name_);
                return line_info;
            }
        }
        return LineInfo::for_function(file, func, line, name_);
    }
    catch (std::out_of_range&)
    {
        // TODO handle better
        // unfortunately this happens often, so it's only debug
        Log::debug() << "could not find section for " << addr << " in " << name_;
        throw LookupError("could not find section", addr);
    }
}

void Lib::read_symbols()
{
    // read this: https://blogs.oracle.com/ali/entry/inside_elf_symbol_tables
    try
    {
        symbols_.resize(check_symtab(bfd_get_symtab_upper_bound(handle_.get())));
        // Note: Keep the extra NULL element at the end for later use!
        symbols_.resize(1 + check_symtab(bfd_canonicalize_symtab(handle_.get(), symbols_.data())));
        // filter_symbols();
        if (symbols_.size() <= 1)
        {
            throw InitError("no symbols in symtab");
        }
    }
    catch (std::exception& e)
    {
        Log::debug() << "failed to get symtab: " << e.what();
        symbols_.reserve(0);
    }
    if (symbols_.size() <= 1)
    {
        try
        {
            // Use dynamic symtab only when regular symbols are not available
            // Usually regular symtab should be a superset. There also doesn't seem to be much
            // of a difference in how to handle them.
            // See linux/tools/perf/util/symbol-elf.c symsrc__init
            Log::debug() << "falling back to .dynsym";

            symbols_.resize(check_symtab(bfd_get_dynamic_symtab_upper_bound(handle_.get())));
            symbols_.resize(
                1 + check_symtab(bfd_canonicalize_dynamic_symtab(handle_.get(), symbols_.data())));
            // filter_symbols();
        }
        catch (std::exception& e)
        {
            Log::debug() << "failed to get dynsym: " << e.what();
            symbols_.reserve(0);
        }
    }
    if (symbols_.size() <= 1)
    {
        throw InitError("could not find any symbols in .symtab or .dynsym", name_);
    }
}

void Lib::read_sections()
{
    // We could read *all* sections using bfd_map_ver_sections, however some sections are
    // overlapping :-(, so we couldn't put them in our range-map. So instead use only the
    // sections that are referenced by a function symbol.
    for (auto sym : symbols_)
    {
        if (sym == nullptr || !(sym->flags & BSF_FUNCTION))
        {
            continue;
        }
        auto section = sym->section;
        if (bfd_is_und_section(section))
        {
            continue;
        }
        // VMA is useless for shared libraries, we remove the offset from the map
        // so use filepos instead
        // auto start = bfd_get_section_vma(handle_, section);
        auto start = section->filepos;
        auto size = bfd_get_section_size(section);
        if (size == 0)
        {
            Log::debug() << "skipping empty section: "
                         << bfd_get_section_name(handle_.get(), section);
            continue;
        }
        try
        {
            auto r = sections_.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(start, start + size),
                                       std::forward_as_tuple(section));
            if (r.second)
            {
                Log::trace() << "Added section: " << bfd_get_section_name(handle_.get(), section)
                             << " from symbol " << sym->name;
            }
        }
        catch (Range::Error& e)
        {
            Log::warn() << "failed to add section " << bfd_get_section_name(handle_.get(), sym)
                        << "due to " << e.what();
        }
    }
}
} // namespace bfdr
} // namespace lo2s
