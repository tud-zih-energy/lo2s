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

#include <lo2s/radare.hpp>

namespace lo2s
{
Radare::Radare() : r_asm_(r_asm_new())
{
    r_asm_use(r_asm_, "x86");
    r_asm_set_bits(r_asm_, 64);
    r_asm_set_syntax(r_asm_, R_ASM_SYNTAX_ATT);
    r_asm_set_pc(r_asm_, 0x0);
}

std::string Radare::single_instruction(char* buf)
{
    auto len = strlen(buf);
    if (len == 0)
    {
        throw Error("empty instruction");
    }
    for (size_t i = 0; i < len; i++)
    {
        if (buf[i] == '\n')
        {
            buf[i] = '\0';
            break;
        }
    }
    return std::string(buf);
}

std::string Radare::operator()(Address ip, std::istream& obj)
{
    std::streampos offset = ip.value();
    if (offset < 0)
    {
        throw Error("cannot read memory above 63bit");
    }

    constexpr size_t max_instr_size = 16;
    char buffer[max_instr_size];
    obj.seekg(offset);
    obj.read(buffer, max_instr_size);
    auto read_bytes = obj.gcount();
    if (read_bytes == 0)
    {
        throw Error("instruction pointer at end of file");
    }
    auto code = r_asm_mdisassemble(r_asm_, (unsigned char*)buffer, read_bytes);
    auto ret = single_instruction(code->buf_asm);
    r_asm_code_free(code);
    return ret;
}

RadareResolver::RadareResolver(const std::string& filename) : obj_(filename)
{
    if (obj_.fail())
    {
        throw std::ios_base::failure("could not open library file.");
    }
    obj_.exceptions(std::ifstream::failbit | std::ifstream::badbit);
}

std::string RadareResolver::instruction(Address ip)
{
    return Radare::instance()(ip, obj_);
}
} // namespace lo2s
