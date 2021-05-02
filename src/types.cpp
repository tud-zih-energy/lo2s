/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <lo2s/execution_scope.hpp>
#include <lo2s/types.hpp>
namespace lo2s
{

ExecutionScope Thread::as_scope() const
{
    return ExecutionScope::thread(tid_);
}

ExecutionScope Process::as_scope() const
{
    return ExecutionScope::process(pid_);
}
Process Thread::as_process() const
{
    return Process(tid_);
}
Thread Process::as_thread() const
{
    return Thread(pid_);
}

int Cpu::as_int() const
{
    return cpu_;
}

ExecutionScope Cpu::as_scope() const
{
    return ExecutionScope::cpu(cpu_);
}

} // namespace lo2s
