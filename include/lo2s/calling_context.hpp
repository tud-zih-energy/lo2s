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
#pragma once

#include <lo2s/address.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/ompt/events.hpp>
#include <lo2s/types.hpp>

#include <otf2xx/otf2.hpp>

#include <otf2xx/otf2.hpp>

namespace lo2s
{

enum class CallingContextType
{
    ROOT,
    PROCESS,
    THREAD,
    SAMPLE_ADDR,
    GPU_KERNEL,
    SYSCALL,
    OPENMP
};

// Type that encodes all the different types of CallingContext we can have
struct CallingContext
{
public:
    static CallingContext process(Process p)
    {
        CallingContext c;
        c.type = CallingContextType::PROCESS;
        c.p = p;
        return c;
    }

    static CallingContext thread(Thread t)
    {
        CallingContext c;
        c.type = CallingContextType::THREAD;
        c.t = t;
        return c;
    }

    static CallingContext sample(Address addr)
    {
        CallingContext c;
        c.type = CallingContextType::SAMPLE_ADDR;
        c.addr = addr;
        return c;
    }

    static CallingContext gpu_kernel(uint64_t kernel_id)
    {
        CallingContext c;
        c.type = CallingContextType::GPU_KERNEL;
        c.kernel_id = kernel_id;
        return c;
    }

    static CallingContext syscall(uint64_t id)
    {
        CallingContext c;
        c.type = CallingContextType::SYSCALL;
        c.syscall_id = id;
        return c;
    }

    static CallingContext openmp(omp::OMPTCctx cctx)
    {
        CallingContext c;
        c.type = CallingContextType::OPENMP;
        c.omp_cctx = cctx;
        return c;
    }

    static CallingContext root()
    {
        CallingContext c;
        c.type = CallingContextType::ROOT;
        return c;
    }

    Address to_addr() const
    {
        if (type == CallingContextType::SAMPLE_ADDR)
        {
            return addr;
        }

        throw std::runtime_error("Not an Address!");
    }

    uint64_t to_syscall_id() const
    {
        if (type == CallingContextType::SYSCALL)
        {
            return syscall_id;
        }

        throw std::runtime_error("Not a system call!");
    }

    uint64_t to_kernel_id() const
    {
        if (type == CallingContextType::GPU_KERNEL)
        {
            return kernel_id;
        }

        throw std::runtime_error("Not a GPU kernel!");
    }

    omp::OMPTCctx to_omp_cctx() const
    {
        if (type == CallingContextType::OPENMP)
        {
            return omp_cctx;
        }
        throw std::runtime_error("Not a OpenMP cctx!");
    }

    Process to_process() const
    {
        if (type == CallingContextType::PROCESS)
        {
            return p;
        }

        throw std::runtime_error("Not a Process!");
    }

    Thread to_thread() const
    {
        if (type == CallingContextType::THREAD)
        {
            return t;
        }

        throw std::runtime_error("Not a Process!");
    }

    CallingContextType type;

    friend bool operator<(const CallingContext& lhs, const CallingContext& rhs)
    {
        if (lhs.type != rhs.type)
        {
            return lhs.type < rhs.type;
        }
        else
        {
            switch (lhs.type)
            {
            case CallingContextType::PROCESS:
                return lhs.p < rhs.p;
            case CallingContextType::THREAD:
                return lhs.t < rhs.t;
            case CallingContextType::GPU_KERNEL:
                return lhs.kernel_id < rhs.kernel_id;
            case CallingContextType::SAMPLE_ADDR:
                return lhs.addr < rhs.addr;
            case CallingContextType::SYSCALL:
                return lhs.syscall_id < rhs.syscall_id;
            case CallingContextType::OPENMP:
                return lhs.omp_cctx < rhs.omp_cctx;
            case CallingContextType::ROOT:
                throw std::runtime_error("Can not have two CallingContext Roots!");
            }
            return false;
        }
    }

    friend bool operator==(const CallingContext& lhs, const CallingContext& rhs)
    {
        if (lhs.type == rhs.type)
        {
            switch (lhs.type)
            {
            case CallingContextType::ROOT:
                throw std::runtime_error("Can not have two CallingContext Roots!");
            case CallingContextType::PROCESS:
                return lhs.p == rhs.p;
            case CallingContextType::THREAD:
                return lhs.t == rhs.t;
            case CallingContextType::SAMPLE_ADDR:
                return lhs.addr == rhs.addr;
            case CallingContextType::GPU_KERNEL:
                return lhs.kernel_id == rhs.kernel_id;
            case CallingContextType::SYSCALL:
                return lhs.syscall_id == rhs.syscall_id;
            case CallingContextType::OPENMP:
                return lhs.omp_cctx == rhs.omp_cctx;
            }
            return false;
        }
        else
        {
            return false;
        }
    }

    friend bool operator!=(const CallingContext& lhs, const CallingContext& rhs)
    {
        if (lhs.type != rhs.type)
        {
            switch (lhs.type)
            {
            case CallingContextType::ROOT:
                throw std::runtime_error("Can not have two CallingContext Roots!");
            case CallingContextType::PROCESS:
                return lhs.p != rhs.p;
            case CallingContextType::THREAD:
                return lhs.t != rhs.t;
            case CallingContextType::GPU_KERNEL:
                return lhs.kernel_id != rhs.kernel_id;
            case CallingContextType::SAMPLE_ADDR:
                return lhs.addr != rhs.addr;
            case CallingContextType::SYSCALL:
                return lhs.syscall_id != rhs.syscall_id;
            case CallingContextType::OPENMP:
                return lhs.omp_cctx != rhs.omp_cctx;
            }
            return false;
        }
        else
        {
            return false;
        }
    }

    std::string name() const
    {
        switch (type)
        {
        case CallingContextType::ROOT:
            return "ROOT";
        case CallingContextType::PROCESS:
            return fmt::format("process {}", p.as_pid_t());
        case CallingContextType::THREAD:
            return fmt::format("thread {}", t.as_pid_t());
        case CallingContextType::SAMPLE_ADDR:
            return fmt::format("sample addr {}", addr);
        case CallingContextType::GPU_KERNEL:
            return fmt::format("gpu kernel {}", kernel_id);
        case CallingContextType::SYSCALL:
            return fmt::format("syscall {}", syscall_id);
        case CallingContextType::OPENMP:
            return fmt::format("openmp {}", omp_cctx.name());
        }
        return "";
    }

private:
    CallingContext() : addr(0)
    {
    }

    union
    {
        Process p;
        Thread t;
        Address addr;
        uint64_t kernel_id;
        uint64_t syscall_id;
        omp::OMPTCctx omp_cctx;
    };
};

// Node type of the tree containing the CallingContext -> local cctx reference number mappings.
struct LocalCctxNode
{
    LocalCctxNode(otf2::definition::calling_context::reference_type r) : ref(r)
    {
    }

    otf2::definition::calling_context::reference_type ref;
    std::map<CallingContext, LocalCctxNode> children;
};

// Node type of the global cctx tree, containing CallingContext -> otf2::cctx mappings.
struct GlobalCctxNode
{
    GlobalCctxNode(otf2::definition::calling_context* cctx) : cctx(cctx)
    {
    }

    otf2::definition::calling_context* cctx;
    std::map<CallingContext, GlobalCctxNode> children;
};

typedef std::map<CallingContext, LocalCctxNode> LocalCctxMap;
typedef std::map<CallingContext, GlobalCctxNode> GlobalCctxMap;

} // namespace lo2s
