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

#include <lo2s/config.hpp>
#include <lo2s/cupti/events.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/ringbuf.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types.hpp>

#include <chrono>
#include <cstdlib>
#include <string>

extern "C"
{
#include <sys/timerfd.h>
#include <unistd.h>
}

namespace lo2s
{
namespace cupti
{

class Reader
{
public:
    Reader(trace::Trace& trace, Process process)
    : process_(process), trace_(trace), time_converter_(perf::time::Converter::instance()),
      ringbuf_reader_("cupti", process.as_pid_t(), true, config().nvidia_ringbuf_size),
      timer_fd_(timerfd_from_ns(config().userspace_read_interval)),
      executable_name_(get_process_exe(process))
    {
    }

    void read()
    {
        struct event_header* header = nullptr;

        while ((header = reinterpret_cast<struct event_header*>(
                    ringbuf_reader_.get(sizeof(struct event_header)))) != nullptr)
        {
            if (header->type == EventType::CUPTI_KERNEL)
            {
                struct event_kernel* kernel =
                    reinterpret_cast<struct event_kernel*>(ringbuf_reader_.get(header->size));

                auto& writer = trace_.cuda_writer(Thread(process_.as_thread()));

                std::string kernel_name = kernel->name;
                auto& cu_cctx = trace_.cuda_calling_context(executable_name_, kernel_name);

                writer.write_calling_context_enter(time_converter_(kernel->start), cu_cctx.ref(),
                                                   2);
                writer.write_calling_context_leave(time_converter_(kernel->end), cu_cctx.ref());
            }

            ringbuf_reader_.pop(header->size);
        }
    }

    int fd()
    {
        return timer_fd_;
    }

private:
    Process process_;
    trace::Trace& trace_;
    perf::time::Converter& time_converter_;
    RingBufReader ringbuf_reader_;
    int timer_fd_;
    std::string executable_name_;
};
} // namespace cupti
} // namespace lo2s
