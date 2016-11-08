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
#ifndef X86_RECORD_OTF2_RADARE_HPP
#define X86_RECORD_OTF2_RADARE_HPP

#include "error.hpp"
#include "log.hpp"
#include "address.hpp"

extern "C" {
#include <r_asm.h>

#include <stdint.h>
#include <unistd.h>
}

namespace lo2s
{

class radare
{
public:
    class error : public std::runtime_error
    {
    public:
        error(const std::string& what) : std::runtime_error(what)
        {
        }
    };

    radare();

    static std::string single_instruction(char* buf);

    std::string operator()(address ip, std::istream& obj);

    static radare& instance()
    {
        static radare r;
        return r;
    }

private:
    RAsm* r_asm_;
};

class radare_resolver
{
public:
    radare_resolver(const std::string& filename);

    std::string instruction(address ip);

private:
    std::ifstream obj_;
};
}
#endif // X86_RECORD_OTF2_RADARE_HPP
