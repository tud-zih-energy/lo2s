// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/instruction_resolver.hpp>

#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

#include <exception>
#include <memory>
#include <string>

namespace lo2s
{

std::shared_ptr<InstructionResolver> instruction_resolver_for(const std::string& filename
                                                              [[maybe_unused]])
{
    std::shared_ptr<InstructionResolver> ir = nullptr;
#ifdef HAVE_RADARE
    if (!known_non_executable(filename))
    {
        try
        {
            ir = RadareInstructionResolver::cache(filename);
        }
        catch (std::exception& e)
        {
            Log::trace() << "Could not open Radare instruction resolver for " << filename << ": "
                         << e.what();
        }
    }
#endif
    return ir;
}
} // namespace lo2s
