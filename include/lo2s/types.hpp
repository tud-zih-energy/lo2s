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

#pragma once

extern "C"
{
#include <sys/types.h>
}

#include <fmt/core.h>

#include <vector>

#include <cstdint>

namespace lo2s
{

class ExecutionScope;
class Process;

class Thread
{
public:
    explicit Thread(pid_t tid) : tid_(tid)
    {
    }

    explicit Thread() : tid_(-1)
    {
    }

    friend bool operator==(const Thread& lhs, const Thread& rhs)
    {
        return lhs.tid_ == rhs.tid_;
    }

    friend bool operator!=(const Thread& lhs, const Thread& rhs)
    {
        return lhs.tid_ != rhs.tid_;
    }

    friend bool operator<(const Thread& lhs, const Thread& rhs)
    {
        return lhs.tid_ < rhs.tid_;
    }

    friend bool operator!(const Thread& thread)
    {
        return thread.tid_ == -1;
    }

    Process as_process() const;
    ExecutionScope as_scope() const;

    static Thread invalid()
    {
        return Thread(-1);
    }
    std::string name() const
    {
        return fmt::format("thread {}", tid_);
    }

    friend std::ostream& operator<<(std::ostream& stream, const Thread& thread)
    {
        return stream << thread.name();
    }

    pid_t as_pid_t() const
    {
        return tid_;
    }

private:
    pid_t tid_;
};

class Process
{
public:
    explicit Process(pid_t pid) : pid_(pid)
    {
    }

    explicit Process() : pid_(-1)
    {
    }

    friend bool operator==(const Process& lhs, const Process& rhs)
    {
        return lhs.pid_ == rhs.pid_;
    }

    friend bool operator!=(const Process& lhs, const Process& rhs)
    {
        return !(lhs == rhs);
    }

    friend bool operator<(const Process& lhs, const Process& rhs)
    {
        return lhs.pid_ < rhs.pid_;
    }

    friend bool operator!(const Process& process)
    {
        return process.pid_ == -1;
    }

    static Process invalid()
    {
        return Process(-1);
    }

    static Process idle()
    {
        return Process(0);
    }

    pid_t as_pid_t() const
    {
        return pid_;
    }

    Thread as_thread() const;
    ExecutionScope as_scope() const;
    std::string name() const
    {
        return fmt::format("process {}", pid_);
    }

    friend std::ostream& operator<<(std::ostream& stream, const Process& process)
    {
        return stream << process.name();
    }

private:
    pid_t pid_;
};

class Cpu
{
public:
    explicit Cpu(int cpuid) : cpu_(cpuid)
    {
    }
    int as_int() const;
    ExecutionScope as_scope() const;

    static Cpu invalid()
    {
        return Cpu(-1);
    }
    friend bool operator==(const Cpu& lhs, const Cpu& rhs)
    {
        return lhs.cpu_ == rhs.cpu_;
    }

    friend bool operator<(const Cpu& lhs, const Cpu& rhs)
    {
        return lhs.cpu_ < rhs.cpu_;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Cpu& cpu)
    {
        return stream << cpu.name();
    }

    std::string name() const
    {
        return fmt::format("cpu {}", cpu_);
    }

private:
    int cpu_;
};

class Core
{
public:
    Core(int core_id, int package_id) : core_id_(core_id), package_id_(package_id)
    {
    }

    static Core invalid()
    {
        return Core(-1, -1);
    }

    friend bool operator==(const Core& lhs, const Core& rhs)
    {
        return (lhs.core_id_ == rhs.core_id_) && (lhs.package_id_ == rhs.package_id_);
    }

    friend bool operator<(const Core& lhs, const Core& rhs)
    {
        if (lhs.package_id_ == rhs.package_id_)
        {
            return lhs.core_id_ < rhs.core_id_;
        }
        return lhs.package_id_ < rhs.package_id_;
    }

    int core_as_int() const
    {
        return core_id_;
    }

    int package_as_int() const
    {
        return package_id_;
    }

private:
    int core_id_;
    int package_id_;
};

class Package
{
public:
    explicit Package(int id) : id_(id)
    {
    }

    static Package invalid()
    {
        return Package(-1);
    }

    friend bool operator==(const Package& lhs, const Package& rhs)
    {
        return lhs.id_ == rhs.id_;
    }
    friend bool operator<(const Package& lhs, const Package& rhs)
    {
        return lhs.id_ < rhs.id_;
    }

    int as_int() const
    {
        return id_;
    }

private:
    int id_;
};
} // namespace lo2s

namespace std
{
template <>
struct hash<lo2s::Thread>
{
    std::size_t operator()(const lo2s::Thread& t) const
    {
        return ((std::hash<pid_t>()(t.as_pid_t())));
    }
};

} // namespace std
