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

#include <lo2s/config.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

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
    uint32_t tid;
    uint64_t time;
    char filename[32];
    char funcname[32];
};

class Writer
{
public:
    Writer(trace::Trace& trace)
    : trace_(trace), time_converter_(time::Converter::instance()), writers_(trace.python_writer())
    {
        for (const auto& cpu : Topology::instance().cpus())
        {
            current_call_level_.emplace(Cpu(cpu.id), 0);
            current_python_threads_.emplace(Cpu(cpu.id), Thread::invalid());
        }
    }

    void write(void* data)
    {
        auto event = static_cast<python_event*>(data);
        otf2::chrono::time_point tp = time_converter_(event->time);

        if (event->type == false) // ENTER
        {
            enter_python_function(Cpu(event->cpu), Thread(event->tid), tp, event->funcname,
                                  event->funcname);
        }
        else
        {
            leave_python_function(Cpu(event->cpu), Thread(event->tid), tp, event->filename,
                                  event->funcname);
        }
    }

private:
    void enter_python_function(Cpu cpu, Thread python_thread, otf2::chrono::time_point& tp,
                               char* filename, char* funcname)
    {
        enter_python_thread(cpu, python_thread, tp);
        current_call_level_[cpu]++;

        otf2::definition::calling_context& function_cctx =
            trace_.python_func_cctx(python_thread, filename, funcname);

        *writers_[cpu] << otf2::event::calling_context_enter(tp, function_cctx,
                                                             current_call_level_[cpu]);
    }

    void leave_python_function(Cpu cpu, Thread python_thread, otf2::chrono::time_point& tp,
                               char* filename, char* funcname)
    {
        otf2::definition::calling_context& function_cctx =
            trace_.python_func_cctx(python_thread, filename, funcname);

        *writers_[cpu] << otf2::event::calling_context_leave(tp, function_cctx);

        current_call_level_[cpu]--;
        if (current_call_level_[cpu] < 1)
        {
            leave_python_thread(cpu, python_thread, tp);
        }
    }

    void enter_python_thread(Cpu cpu, Thread python_thread, otf2::chrono::time_point& tp)
    {
        if (current_python_threads_[cpu] == python_thread)
        {
            return;
        }
        if (current_python_threads_[cpu] != Thread::invalid())
        {
            leave_python_thread(cpu, current_python_threads_[cpu], tp);
        }

        otf2::definition::calling_context& python_context = trace_.python_cctx(python_thread);

        *writers_[cpu] << otf2::event::calling_context_enter(tp, python_context, 0);
        current_python_threads_[cpu] = python_thread;
    }

    void leave_python_thread(Cpu cpu, Thread python_thread, otf2::chrono::time_point& tp)
    {
        if (current_python_threads_[cpu] == Thread::invalid())
        {
            return;
        }

        otf2::definition::calling_context& python_context = trace_.python_cctx(python_thread);

        *writers_[cpu] << otf2::event::calling_context_leave(tp, python_context);
        current_call_level_[cpu] = 0;
        current_python_threads_[cpu] = Thread::invalid();
    }
    trace::Trace& trace_;
    time::Converter time_converter_;
    std::map<Cpu, Thread> current_python_threads_;
    std::map<Cpu, int> current_call_level_;
    std::map<Cpu, otf2::writer::local*> writers_;
};
} // namespace bpf_python
} // namespace perf
} // namespace lo2s
