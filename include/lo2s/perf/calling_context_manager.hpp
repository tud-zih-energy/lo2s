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
    CallingContextManager(trace::Trace& trace) : local_cctx_refs_(trace.create_cctx_refs())
    {
    }

    void thread_enter(Process process, Thread thread)
    {
        auto ret =
            local_cctx_refs_.map.emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                         std::forward_as_tuple(process, next_cctx_ref_));
        if (ret.second)
        {
            next_cctx_ref_++;
        }

        current_thread_cctx_refs_ = &(*ret.first);
    }

    void finalize(otf2::writer::local* otf2_writer)
    {
        local_cctx_refs_.ref_count = next_cctx_ref_;
        // set writer last, because it is used as sentry to confirm that the cctx refs are properly
        // finalized.
        local_cctx_refs_.writer = otf2_writer;
    }

    bool thread_changed(Thread thread)
    {
        return !current_thread_cctx_refs_ || current_thread_cctx_refs_->first != thread;
    }

    otf2::definition::calling_context::reference_type current()
    {
        if (current_thread_cctx_refs_)
        {
            return current_thread_cctx_refs_->second.entry.ref;
        }
        else
        {
            return otf2::definition::calling_context::reference_type::undefined();
        }
    }

    otf2::definition::calling_context::reference_type sample_ref(uint64_t num_ips, const perf_branch_entry entries[])
    {
        auto children = &current_thread_cctx_refs_->second.entry.children;
        for (uint64_t i = num_ips - 1;num_ips > 1; i--)
        {
            auto it = find_ip_child(entries[i].to, *children);
            // We intentionally discard the last sample as it is somewhere in the kernel
            if (i == 1)
            {
                return it->second.ref;
            }

            children = &it->second.children;
        }

        return otf2::definition::calling_context::reference_type::undefined();
    }

    otf2::definition::calling_context::reference_type sample_ref(uint64_t num_ips,
                                                                 const uint64_t ips[])
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
                return it->second.ref;
            }

            children = &it->second.children;
        }
    }

    otf2::definition::calling_context::reference_type sample_ref(uint64_t ip)
    {
        auto it = find_ip_child(ip, current_thread_cctx_refs_->second.entry.children);

        return it->second.ref;
    }

    void thread_leave(Thread thread)
    {
        assert(current_thread_cctx_refs_);
        if (current_thread_cctx_refs_->first != thread)
        {
            Log::debug() << "inconsistent leave thread"; // will probably set to trace sooner or
                                                         // later
        }
        current_thread_cctx_refs_ = nullptr;
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
    trace::ThreadCctxRefMap& local_cctx_refs_;
    size_t next_cctx_ref_ = 0;
    trace::ThreadCctxRefMap::value_type* current_thread_cctx_refs_ = nullptr;
};
} // namespace perf
} // namespace lo2s
