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

#include <sys/types.h>
#include <iostream>

namespace lo2s
{

class PidWrapper
{
public:
    pid_t as_pid_t()
    {
        return pid_;
    }
    
    friend bool operator==(const PidWrapper& lhs, const PidWrapper& rhs)
    {
        return lhs.pid_ == rhs.pid_;
    }

        friend bool operator<(const PidWrapper& lhs, const PidWrapper& rhs)
        {
            return lhs.pid_ < rhs.pid_;
        }

        friend bool operator!(const PidWrapper& wrapper)
        {
            return pid_ == -1;
        }
protected:
    PidWrapper(pid_t pid) : pid_(pid)
    {
    }
    PidWrapper() : pid_(-1)
    {
    }

    pid_t pid_;
};

class Thread : public PidWrapper
{
public:
    explicit Thread(pid_t tid) : PidWrapper(tid)
    {
    }
};

class Process : public PidWrapper
{
public:
    explicit Process(pid_t pid) : PidWrapper(pid)
    {
    }

    Thread as_thread()
    {
        return Thread(pid_);
    }
    
    friend std::ostream &operator<<(std::ostream &os, const Process &process)
    {
        return os << "process " << process.pid_;
    }
};

class Cpu
{
    public:
        explicit Cpu(int cpuid) : cpu_(cpuid)
    {
    }
        int as_cpuid()
        {
            return cpu_;
        }

        friend bool operator==(const Cpu& lhs, const Cpu& rhs)
        {
        return lhs.cpu_ == rhs.cpu_;
        }

        friend bool operator <(const Cpu& lhs, const Cpu& rhs)
        {
            return lhs.cpu_ < rhs.cpu_;
        }
    private:
        int cpu_;
};

}
