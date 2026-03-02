// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/address.hpp>

#include <fstream>
#include <istream>
#include <stdexcept>
#include <string>

extern "C"
{
#include <r_anal.h>
#include <r_asm.h>
#include <r_lib.h>
}

namespace lo2s
{

class Radare
{
public:
    class Error : public std::runtime_error
    {
    public:
        Error(const std::string& what) : std::runtime_error(what)
        {
        }
    };

    Radare();

    static std::string single_instruction(char* buf);

    std::string operator()(Address ip, std::istream& obj);

    static Radare& instance()
    {
        static Radare r;
        return r;
    }

private:
    RLib* r_lib_;
    RAnal* r_anal_;
    RAsm* r_asm_;
};

class RadareResolver
{
public:
    RadareResolver(const std::string& filename);

    std::string instruction(Address ip);

private:
    std::ifstream obj_;
};
} // namespace lo2s
