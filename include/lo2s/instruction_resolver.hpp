// SPDX-FileCopyrightText: 2024 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/address.hpp>
#ifdef HAVE_RADARE
#include <lo2s/radare.hpp>
#endif
#include <lo2s/util.hpp>

#include <memory>
#include <string>

namespace lo2s
{
class InstructionResolver
{
public:
    InstructionResolver() = default;
    virtual ~InstructionResolver() = default;

    InstructionResolver(InstructionResolver&) = delete;
    InstructionResolver(InstructionResolver&&) = delete;
    InstructionResolver& operator=(InstructionResolver&) = delete;
    InstructionResolver& operator=(InstructionResolver&&) = delete;

    static InstructionResolver& cache()
    {
        static InstructionResolver ir;
        return ir;
    }

    virtual std::string lookup_instruction(Address addr [[maybe_unused]])
    {
        return "";
    }
};
#ifdef HAVE_RADARE
class RadareInstructionResolver : public InstructionResolver
{
public:
    RadareInstructionResolver(const std::string& name) : radare_(name)
    {
    }

    static std::shared_ptr<RadareInstructionResolver> cache(const std::string& name)
    {
        return BinaryCache<RadareInstructionResolver>::instance()[name];
    }

    std::string lookup_instruction(Address ip) override
    {
        return radare_.instruction(ip);
    }

    RadareResolver radare_;
};
#endif // HAVE_RADARE

std::shared_ptr<InstructionResolver> instruction_resolver_for(const std::string& filename);
} // namespace lo2s
