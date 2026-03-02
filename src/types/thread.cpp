// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/types/thread.hpp>

#include <lo2s/execution_scope.hpp>
#include <lo2s/types/process.hpp>

namespace lo2s
{
ExecutionScope Thread::as_scope() const
{
    return ExecutionScope(*this);
}

Process Thread::as_process() const
{
    return Process(tid_);
}
} // namespace lo2s
