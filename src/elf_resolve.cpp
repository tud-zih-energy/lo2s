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
#include <lo2s/elf_resolve.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>

#include <cstdarg>
#include <cstdlib>

extern "C"
{
#include <fcntl.h>
#include <libelf.h>
#include <unistd.h>
}

namespace lo2s
{
namespace elfr
{

static void read_section(int fd, Elf64_Shdr* shdr, std::vector<char>& buf)
{
    if (lseek(fd, shdr->sh_offset, SEEK_SET) == -1)
    {
        Log::error() << strerror(errno);
        throw_errno();
    }

    if (read(fd, buf.data(), shdr->sh_size) != shdr->sh_size)
    {
        Log::error() << strerror(errno);
        throw_errno();
    }
}

Elf::Elf(const std::string& name) : name_(name)
{
    elf_version(EV_CURRENT);

    int fd = open(name.c_str(), 0, O_RDONLY);

    ::Elf* elf_file = elf_begin(fd, ELF_C_READ, NULL);

    if (elf_file == nullptr)
    {
        throw std::runtime_error(std::string(elf_errmsg(elf_errno())));
    }

    std::map<uint64_t, std::vector<char>> strtab_cache_;

    Elf_Scn* section = NULL;

    while ((section = elf_nextscn(elf_file, section)) != NULL)
    {
        Elf64_Shdr* shdr = elf64_getshdr(section);
        if (shdr->sh_type != SHT_DYNSYM && shdr->sh_type != SHT_SYMTAB)
        {
            continue;
        }
        if (strtab_cache_.count(shdr->sh_link) == 0)
        {
            Elf64_Shdr* sym_strtab_hdr = elf64_getshdr(elf_getscn(elf_file, shdr->sh_link));

            auto res = strtab_cache_.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(shdr->sh_link),
                                             std::forward_as_tuple(sym_strtab_hdr->sh_size));
            read_section(fd, sym_strtab_hdr, res.first->second);
        }
        auto& strtab = strtab_cache_.at(shdr->sh_link);

        assert(shdr->sh_size % sizeof(Elf64_Sym) == 0);

        std::vector<char> symtab(shdr->sh_size);

        read_section(fd, shdr, symtab);
        for (size_t index = 0; index < (shdr->sh_size / sizeof(Elf64_Sym)); index++)
        {
            Elf64_Sym* symbol = &((Elf64_Sym*)symtab.data())[index];
            if (symbol->st_info & SHN_UNDEF)
            {
                continue;
            }
            char* function_name = &strtab.data()[symbol->st_name];

            if (symbol->st_size == 0)
            {
                continue;
            }
            symbols_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(symbol->st_value, symbol->st_value + symbol->st_size),
                std::forward_as_tuple(LineInfo::for_function(function_name, "", 0, "")));
        }
    }
}

LineInfo Elf::lookup(Address addr) const
{
    Log::error() << "Lookup called!";
    return symbols_.at(addr);
}

} // namespace elfr
} // namespace lo2s
