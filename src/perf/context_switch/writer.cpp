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

#include <lo2s/perf/context_switch/writer.hpp>

#include <lo2s/summary.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace perf
{
namespace context_switch
{

Writer::Writer(int cpu, trace::Trace& trace)
 : Reader(cpu),
   otf2_writer_(trace.cpu_writer(cpu)),
   time_converter_(time::Converter::instance()),
   trace_(trace)
{
}

Writer::~Writer()
{
    if(!thread_region_refs_.empty())
    {
        const auto& mapping = trace_.merge_tids(thread_region_refs_);
        otf2_writer_ << mapping;
    }
}

bool Writer::handle(const Reader::RecordSwitchCpuWideType *context_switch)
{
    if(context_switch->next_prev_pid == 0)
    {
        return false;
    }

    auto tp = time_converter_(context_switch->time);
    auto elem =  thread_region_refs_.emplace(std::piecewise_construct, std::forward_as_tuple(context_switch->next_prev_pid), std::forward_as_tuple(thread_region_refs_.size()));

    //Check if we left the region
    if((context_switch->header.misc & PERF_RECORD_MISC_SWITCH_OUT) && first_enter_ == true)
    {
        otf2_writer_.write_leave(tp, elem.first->second);
    }
    else
    {
        //Vampir doesn't like it when records start with a LEAVE
        first_enter_ = true;
        otf2_writer_.write_enter(tp, elem.first->second);

        summary().register_process(context_switch->next_prev_pid);
    }

    return false;
}
}
}
}
