// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/types/process.hpp>

#include <lo2s/execution_scope.hpp>
#include <lo2s/types/thread.hpp>

namespace lo2s
{

ExecutionScope Process::as_scope() const
{
    return ExecutionScope(*this);
}

Thread Process::as_thread() const
{
    return Thread(pid_);
}

} // namespace lo2s
