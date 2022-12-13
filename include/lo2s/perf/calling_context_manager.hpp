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
#include <lo2s/trace/trace.hpp>

#include <otf2xx/otf2.hpp>

namespace lo2s
{
namespace perf
{

class CallingContextManager
{
public:
    CallingContextManager(trace::Trace& trace, otf2::writer::local& writer)
    : trace_(trace), otf2_writer_(writer), local_cctx_refs_(trace.create_cctx_refs())
    {
    }

    void update(otf2::chrono::time_point tp, Process process, Thread thread)
    {
        if (current_thread_cctx_refs_ && current_thread_cctx_refs_->first == thread)
        {
            return;
        }
        else if (current_thread_cctx_refs_)
        {
            leave(tp, thread);
        }

        // thread has changed
        auto ret =
            local_cctx_refs_.map.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                         std::forward_as_tuple(process, next_cctx_ref_));
        if (ret.second)
        {
            next_cctx_ref_++;
        }
        otf2_writer_.write_calling_context_enter(tp, ret.first->second.entry.ref, 2);
        current_thread_cctx_refs_ = &(*ret.first);
    }

    void leave(otf2::chrono::time_point tp, Thread thread)
    {
        if (current_thread_cctx_refs_ == nullptr)
        {
            // Already not in a thread
            return;
        }

        if (current_thread_cctx_refs_->first != thread)
        {
            Log::debug() << "inconsistent leave thread"; // will probably set to trace sooner or
                                                         // later
        }
        otf2_writer_.write_calling_context_leave(tp, current_thread_cctx_refs_->second.entry.ref);
        current_thread_cctx_refs_ = nullptr;
    }

    void finalize(otf2::chrono::time_point tp)
    {
        if (current_thread_cctx_refs_)
        {
            otf2_writer_.write_calling_context_leave(tp,
                                                     current_thread_cctx_refs_->second.entry.ref);
        }
        local_cctx_refs_.ref_count = next_cctx_ref_;
        // set writer last, because it is used as sentry to confirm that the cctx refs are properly
        // finalized.
        local_cctx_refs_.writer = &otf2_writer_;
    }

    void write_sample(otf2::chrono::time_point tp, uint64_t num_ips, const uint64_t ips[])
    {
        // For unwind distance definiton, see:
        // http://scorepci.pages.jsc.fz-juelich.de/otf2-pipelines/docs/otf2-2.2/html/group__records__definition.
        // html#CallingContext

        // We always have a fake CallingContext for the process, which is scheduled.
        // So if we don't record the calling context tree, all nodes in the calling context tree
        // are: The fake node for the process and a second node for the current context of the
        // sample. Therefore, we always have an unwind distance of 2. (Technically, it could also be
        // 1, but we lack the information to distinguish those cases. Hence, we stick with 2.)
        //
        // However, in the case that we actually record the complete call stack, we get the number
        // of nodes in the calling context tree from the number of elements in the call stack
        // recorded from perf. Tough, we still have the fake calling context for the process, which
        // adds 1 to that number. But we also remove the lowest entry from the call stack, because
        // that is ALWAYS in the kernel, which can't be resolved and thus doesn't provide any useful
        // information.
        //
        // Having these things in mind, look at this line and tell me, why it is still wrong:
        auto children = &current_thread_cctx_refs_->second.entry.children;
        for (uint64_t i = num_ips - 1;; i--)
        {
            auto it = find_ip_child(ips[i], *children);
            // We intentionally discard the last sample as it is somewhere in the kernel
            if (i == 1)
            {
                otf2_writer_.write_calling_context_sample(tp, it->second.ref, num_ips,
                                                          trace_.interrupt_generator().ref());
                return;
            }

            children = &it->second.children;
        }
    }

    void write_sample(otf2::chrono::time_point tp, uint64_t ip)
    {
        auto it = find_ip_child(ip, current_thread_cctx_refs_->second.entry.children);

        otf2_writer_.write_calling_context_sample(tp, it->second.ref, 2,
                                                  trace_.interrupt_generator().ref());
    }

private:
    trace::IpRefMap::iterator find_ip_child(Address addr, trace::IpRefMap& children)
    {
        // -1 can't be inserted into the ip map, as it imples a 1-byte region from -1 to 0.
        if (addr == -1)
        {
            Log::debug() << "Got invalid ip (-1) from call stack. Replacing with -2.";
            addr = -2;
        }
        auto ret = children.emplace(std::piecewise_construct, std::forward_as_tuple(addr),
                                    std::forward_as_tuple(next_cctx_ref_));
        if (ret.second)
        {
            next_cctx_ref_++;
        }
        return ret.first;
    }

private:
    trace::Trace& trace_;
    otf2::writer::local& otf2_writer_;
    trace::ThreadCctxRefMap& local_cctx_refs_;
    trace::ThreadCctxRefMap::value_type* current_thread_cctx_refs_ = nullptr;

    size_t next_cctx_ref_ = 0;
};
} // namespace perf
} // namespace lo2s
