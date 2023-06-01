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
Radare::Radare() : r_lib_(r_lib_new(nullptr, nullptr)), r_anal_(r_anal_new()), r_asm_(r_asm_new())
{
    r_unref(r_anal_->config);

    r_asm_->num = r_num_new(nullptr, nullptr, nullptr);
    r_anal_->config = r_ref_ptr(r_asm_->config);
    r_anal_bind(r_anal_, &r_asm_->analb);

    r_asm_use(r_asm_, R_SYS_ARCH);
    r_anal_use(r_anal_, R_SYS_ARCH);

    int sysbits = (R_SYS_BITS & R_SYS_BITS_64) ? 64 : 32;
    r_asm_set_bits(r_asm_, sysbits);
    r_anal_set_bits(r_anal_, sysbits);
}

std::string Radare::single_instruction(char* buf)
{
    if (buf == nullptr)
    {
        throw Error("code->assembly is NULL");
    }

    auto it = buf;

    while (*it != '\0' && *it != '\n')
    {
        it++;
    }

    if (it == buf)
    {
        throw Error("empty instruction");
    }

    return std::string(buf, it - 1);
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

    r_asm_set_pc(r_asm_, offset);
    auto code = r_asm_mdisassemble(r_asm_, (unsigned char*)buffer, read_bytes);

    if (code == nullptr)
    {
        throw Error("could not disassemble instruction");
    }

    auto ret = single_instruction(code->assembly);

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
