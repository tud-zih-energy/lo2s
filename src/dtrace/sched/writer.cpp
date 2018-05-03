/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

#include <lo2s/dtrace/sched/writer.hpp>

#include <lo2s/dtrace/event_reader.hpp>

#include <lo2s/trace/trace.hpp>

#include <lo2s/summary.hpp>

#include <cstring>
#include <sstream>

extern "C"
{
#include <dtrace.h>
}

namespace lo2s
{
namespace dtrace
{
namespace sched
{

Writer::Writer(int cpu, trace::Trace& trace)
: EventReader(), otf2_writer_(trace.cpu_writer(cpu)), trace_(trace),
  time_converter_(time::Converter::instance())
{
    // lets add a special name for the kernel here
    comms_[0] = "kernel_task";

    std::stringstream str;

    str << "sched:::on-cpu / cpu == " << cpu
        << " / { trace(walltimestamp); trace(pid); trace(tid); trace(curlwpsinfo->pr_pri); } "
        << "proc:::exit / cpu == " << cpu << " / { printf(\"%d %s\", pid, execname); }";

    init_dtrace_program(str.str());
}

Writer::~Writer()
{
    if (!thread_region_refs_.empty())
    {
        const auto& mapping = trace_.merge_tids(thread_region_refs_);
        otf2_writer_ << mapping;
    }
}

void Writer::handle(int, const ProbeDesc& pd, nitro::lang::string_ref data, void* raw_data)
{

    if (pd.provider == "sched")
    {
        if (data_.i == 0)
        {
            data_.ts = *(long long*)raw_data;
            ++data_.i;
            return;
        }
        else if (data_.i == 1)
        {
            data_.pid = *(int*)raw_data;
            ++data_.i;
            return;
        }
        else if (data_.i == 2)
        {
            data_.tid = *(int*)raw_data;
            ++data_.i;
            return;
        }
        else
        {
            data_.prio = *(int*)raw_data;
            data_.i = 0;
        }

        if (!current_region_.is_undefined())
        {
            otf2_writer_.write_leave(time_converter_(data_.ts), current_region_);
        }

        current_region_ = current_region_.undefined();

        // pid 0 also comprises the threads used as idle loops.
        // It seems though, they are (almost) the only threads with a priority of 0.
        // So we check for that here and hope for the best...
        if (data_.pid != 0 || (data_.pid == 0 && data_.prio != 0))
        {
            current_region_ = thread_region_ref(data_.pid);
            otf2_writer_.write_enter(time_converter_(data_.ts), current_region_);
        }
    }
    else
    {
        std::stringstream str(data);

        int pid;
        std::string comm;
        str >> pid >> comm;

        summary().register_process(pid);
        comms_[pid] = comm;
    }
}

otf2::definition::region::reference_type Writer::thread_region_ref(pid_t tid)
{
    tid %= 10000000;
    auto it = thread_region_refs_.find(tid);
    if (it == thread_region_refs_.end())
    {
        // If we ever merge interrupt samples and switch events we must use a common counter here!
        otf2::definition::region::reference_type ref(thread_region_refs_.size());
        auto ret = thread_region_refs_.emplace(tid, ref);
        assert(ret.second);
        it = ret.first;
    }
    return it->second;
}

void Writer::merge_trace()
{
    trace_.register_tids(comms_);
}

} // namespace sched
} // namespace dtrace
} // namespace lo2s
