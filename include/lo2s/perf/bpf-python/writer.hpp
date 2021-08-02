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

#include <lo2s/trace/trace.hpp>
#include <lo2s/config.hpp>

#include <cstdint>
namespace lo2s
{
namespace perf
{
namespace bpf_python
{
  struct python_event
  {
      bool type;
      int cpu;
      uint64_t time;
      char filename[32];
      char funcname[16];
  };

class Writer
{
public:
    Writer(trace::Trace& trace) :
        writers_(trace.python_writer())
    {
        ident = 0;
    }
    
    void write(void *data)
    {
     auto event = static_cast<python_event*>(data);
     if (event->type == false) // ENTER
     {
         for (int i = 0; i < ident; i++)
         {
             std::cout << " ";
         }
         std::cout << "ENTER: (" << event->time << "):" << event->filename << "." << event->funcname
                   << "(ON CPU:" << event->cpu << ")" << std::endl;
         ident++;
     }
     else
     {
         ident--;
         if (ident < 0)
         {
             ident = 0;
         }

         for (int i = 0; i < ident; i++)
         {
             std::cout << " ";
         }
         std::cout << "LEAVE: (" << event->time << "):" << event->filename << "." << event->funcname
                   << std::endl;
     }
    }
private:
    int ident;
std::vector<otf2::writer::local*> writers_;
};
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
