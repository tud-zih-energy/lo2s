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
#include <lo2s/types.hpp>

namespace lo2s
{
enum class CallingContextType
{
    ROOT,
    PROCESS,
    THREAD,
    SAMPLE_ADDR,
    CUDA
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

    static CallingContext cuda(Address addr)
    {
        CallingContext c;
        c.type = CallingContextType::CUDA;
        c.addr = addr;
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
        if (type == CallingContextType::SAMPLE_ADDR || type == CallingContextType::CUDA)
        {
            return addr;
        }

        throw std::runtime_error("Not an Address!");
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
            case CallingContextType::CUDA:
            case CallingContextType::SAMPLE_ADDR:
                return lhs.addr < rhs.addr;
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
                return true;
            case CallingContextType::PROCESS:
                return lhs.p == rhs.p;
            case CallingContextType::THREAD:
                return lhs.t == rhs.t;
            case CallingContextType::SAMPLE_ADDR:
            case CallingContextType::CUDA:
                return lhs.addr == rhs.addr;
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
                return false;
            case CallingContextType::PROCESS:
                return lhs.p != rhs.p;
            case CallingContextType::THREAD:
                return lhs.t != rhs.t;
            case CallingContextType::SAMPLE_ADDR:
            case CallingContextType::CUDA:
                return lhs.addr != rhs.addr;
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
            return fmt::format("PROCESS {}", p.as_pid_t());
        case CallingContextType::THREAD:
            return fmt::format("THREAD {}", t.as_pid_t());
            ;
        case CallingContextType::SAMPLE_ADDR:
            return fmt::format("SAMPLE ADDR {}", addr);
            ;
        case CallingContextType::CUDA:
            return fmt::format("CUDA KERNEL {}", addr.value());
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
    };
};
} // namespace lo2s
