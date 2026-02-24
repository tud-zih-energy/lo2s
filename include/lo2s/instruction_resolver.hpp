/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
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
