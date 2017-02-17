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

#include <cstdlib>

namespace lo2s
{
const std::string line_info::unknown_str = "(unknown)";

namespace bfdr
{
    initializer lib::dummy_;

    lib::lib(const std::string& name) : name_(name)
    {
        handle_ = bfd_openr(name.c_str(), nullptr);
        if (handle_ == nullptr)
        {
            throw init_error("failed to open BFD handle", name);
        }
        // init stuff
        //  Not sure if those are needed for side-effects
        bfd_get_arch_size(handle_);
        bfd_find_target(nullptr, handle_);
        if (!bfd_check_format(handle_, bfd_object))
        {
            bfd_close(handle_);
            handle_ = nullptr;
            throw init_error("Not a valid bfd_object", name);
        }

        char** matching;
        if (bfd_check_format_matches(handle_, bfd_archive, &matching))
        {
            bfd_close(handle_);
            handle_ = nullptr;
            throw init_error("BFD format does not match.", name);
        }

        //            if (bfd_get_flavour(handle_) != bfd_target_elf_flavour)
        //            {
        //                log::error() << "Only supporting elf right now. Need symbol size.";
        //                throw std::runtime_error("BFD flavor not ELF.");
        //            }

        try
        {
            read_symbols();
            read_sections();
        }
        catch (...)
        {
            bfd_close(handle_);
            handle_ = nullptr;
            throw;
        }
    }

    lib::~lib()
    {
        if (handle_ != nullptr)
        {
            bfd_close(handle_);
            handle_ = nullptr;
        }
    }

    line_info lib::lookup(address addr) const
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
                log::error() << "address " << addr << " out of section bounds " << name_ << " "
                             << bfd_get_section_name(handle_, section) << " filepos: " << hex
                             << base << ", size: " << hex << size;
                throw lookup_error("address out of .symtab bounds", addr);
            }
            auto evil_symtab = const_cast<asymbol**>(symbols_.data());
            auto found = bfd_find_nearest_line(handle_, section, evil_symtab, local_addr.value(),
                                               &file, &func, &line);
            // we probably don't need to free here.
            if (!found)
            {
                log::debug() << "bfd_find_nearest_line failed " << addr << " in " << name_
                             << bfd_get_section_name(handle_, section);
                throw lookup_error("bfd_find_nearest_line failed", addr);
            }
            if (func != nullptr)
            {
                auto demangled = bfd_demangle(handle_, func, DMGL_PARAMS | DMGL_ANSI);
                if (demangled != nullptr)
                {
                    std::string demangled_str(demangled);
                    free(demangled);
                    return line_info(file, demangled_str.c_str(), line, name_);
                }
            }
            return line_info(file, func, line, name_);
        }
        catch (std::out_of_range&)
        {
            // TODO handle better
            // unfortunately this happens often, so it's only debug
            log::debug() << "could not find section for " << addr << " in " << name_;
            throw lookup_error("could not find section", addr);
        }
    }

    void lib::read_symbols()
    {
        // read this: https://blogs.oracle.com/ali/entry/inside_elf_symbol_tables
        try
        {
            symbols_.resize(check_symtab(bfd_get_symtab_upper_bound(handle_)));
            // Note: Keep the extra NULL element at the end for later use!
            symbols_.resize(1 + check_symtab(bfd_canonicalize_symtab(handle_, symbols_.data())));
            // filter_symbols();
            if (symbols_.size() <= 1)
            {
                throw init_error("no symbols in symtab");
            }
        }
        catch (init_error& e)
        {
            log::debug() << "failed to get symtab: " << e.what();
            symbols_.reserve(0);
        }
        if (symbols_.size() <= 1)
        {
            // Use dynamic symtab only when regular symbols are not available
            // Usually regular symtab should be a superset. There also doesn't seem to be much
            // of a difference in how to handle them.
            // See linux/tools/perf/util/symbol-elf.c symsrc__init
            log::debug() << "falling back to .dynsym";

            symbols_.resize(check_symtab(bfd_get_dynamic_symtab_upper_bound(handle_)));
            symbols_.resize(
                1 + check_symtab(bfd_canonicalize_dynamic_symtab(handle_, symbols_.data())));
            // filter_symbols();
        }
        if (symbols_.size() <= 1)
        {
            throw init_error("could not find any symbols in .symtab or .dynsym", name_);
        }
    }

    void lib::read_sections()
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
                log::debug() << "skipping empty section: "
                             << bfd_get_section_name(handle_, section);
                continue;
            }
            try
            {
                auto r = sections_.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(start, start + size),
                                           std::forward_as_tuple(section));
                if (r.second)
                {
                    log::trace() << "Added section: " << bfd_get_section_name(handle_, section)
                                 << " from symbol " << sym->name;
                }
            }
            catch (range::error& e)
            {
                log::warn() << "failed to add section " << bfd_get_section_name(handle_, sym)
                            << "due to " << e.what();
            }
        }
    }
}
}
