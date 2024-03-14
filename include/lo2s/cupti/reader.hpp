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
#include <lo2s/cupti/ringbuf.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/time/converter.hpp>
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
      ringbuf_reader_(std::to_string(process.as_pid_t()), 512)
    {
        struct itimerspec tspec;
        memset(&tspec, 0, sizeof(struct itimerspec));
        tspec.it_value.tv_nsec = 1;

        tspec.it_interval.tv_sec =
            std::chrono::duration_cast<std::chrono::seconds>(config().userspace_read_interval)
                .count();

        tspec.it_interval.tv_nsec =
            (config().userspace_read_interval % std::chrono::seconds(1)).count();
        timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

        timerfd_settime(timer_fd_, TFD_TIMER_ABSTIME, &tspec, NULL);
        exe = get_process_exe(process);
    }

    void read()
    {
        rb_ptr header_rb = ringbuf_reader_.get(sizeof(struct cupti_event_header));

        if (!header_rb)
        {
            return;
        }

        struct cupti_event_header* header = (struct cupti_event_header*)header_rb.get();

        if (header->type == CuptiEventType::CUPTI_KERNEL)
        {
            rb_ptr kernel_rb = ringbuf_reader_.get(header->size);
            struct cupti_event_kernel* kernel = (struct cupti_event_kernel*)kernel_rb.get();

            auto& writer = trace_.cuda_writer(Thread(process_.as_thread()));

            std::string kernel_name = kernel->name;
            auto& cu_cctx = trace_.cuda_calling_context(exe, kernel_name);

            writer << otf2::event::calling_context_enter(time_converter_(kernel->start), cu_cctx,
                                                         2);
            writer << otf2::event::calling_context_leave(time_converter_(kernel->end), cu_cctx);

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
    std::string exe;
};
} // namespace cupti
} // namespace lo2s
