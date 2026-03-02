// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/execution_scope.hpp>

#include <lo2s/types/cpu.hpp>
#include <lo2s/types/process.hpp>
#include <lo2s/types/thread.hpp>

#include <cassert>

namespace lo2s
{

ExecutionScope::ExecutionScope(Thread thread)
: type(ExecutionScopeType::THREAD), id(thread.as_int())
{
}

ExecutionScope::ExecutionScope(Process process)
: type(ExecutionScopeType::PROCESS), id(process.as_int())
{
}

ExecutionScope::ExecutionScope(Cpu cpu) : type(ExecutionScopeType::CPU), id(cpu.as_int())
{
}

Thread ExecutionScope::as_thread() const
{
    assert(type == ExecutionScopeType::THREAD);
    return Thread(id);
}

Process ExecutionScope::as_process() const
{
    assert(type == ExecutionScopeType::PROCESS);
    return Process(id);
}

Cpu ExecutionScope::as_cpu() const
{
    assert(type == ExecutionScopeType::CPU);
    return Cpu(id);
}
} // namespace lo2s
