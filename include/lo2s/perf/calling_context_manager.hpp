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
#include <lo2s/log.hpp>

#include <otf2xx/otf2.hpp>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{

struct LocalCctx
{
    LocalCctx(otf2::definition::calling_context::reference_type r) : ref(r)
    {
    }

    otf2::definition::calling_context::reference_type ref;
    std::map<Address, LocalCctx> children;
};

class LocalCctxMap
{
public:
    LocalCctxMap()
    {
    }

    void finalize(otf2::writer::local* otf2_writer)
    {
        ref_count_ = next_cctx_ref_;
        // set writer last, because it is used as sentry to confirm that the cctx refs are properly
        // finalized.
        writer_ = otf2_writer;
    }

    otf2::definition::calling_context::reference_type sample_ref(Process p, uint64_t num_ips,
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
        auto children = &map[p];
        uint64_t ref = -1;
        for (uint64_t i = num_ips - 1;; i--)
        {
            if (ips[i] == PERF_CONTEXT_KERNEL)
            {
                if (i <= 1)
                {
                    return ref;
                }
                continue;
            }
            else if (ips[i] == PERF_CONTEXT_USER)
            {
                if (i <= 1)
                {
                    return ref;
                }
                continue;
            }
            auto it = find_ip_child(ips[i], *children);
            ref = it->second.ref;
            if (i == 0)
            {
                return ref;
            }

            children = &it->second.children;
        }
    }

    otf2::definition::calling_context::reference_type sample_ref(Process p, uint64_t ip)
    {
        auto it = find_ip_child(ip, map[p]);

        return it->second.ref;
    }

    otf2::definition::calling_context::reference_type thread(Process process, Thread thread)
    {
        auto ret =
            thread_cctxs_[process].emplace(std::piecewise_construct, std::forward_as_tuple(thread),
                                           std::forward_as_tuple(next_cctx_ref_));
        if (ret.second)
        {
            next_cctx_ref_++;
        }

        return ret.first->second.ref;
    }

    size_t num_cctx() const
    {
        return ref_count_;
    }

    const std::map<Process, std::map<Thread, LocalCctx>>& get_threads() const
    {
        return thread_cctxs_;
    }

    const std::map<Process, std::map<Address, LocalCctx>>& get_functions() const
    {
        return map;
    }

    otf2::writer::local* writer()
    {
        return writer_;
    }

private:
    std::map<Address, LocalCctx>::iterator find_ip_child(Address addr,
                                                         std::map<Address, LocalCctx>& children)
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
    std::map<Process, std::map<Address, LocalCctx>> map;
    std::map<Process, std::map<Thread, LocalCctx>> thread_cctxs_;

    /*
     * Stores calling context information for each sample writer / monitoring thread.
     * While the `Trace` always owns this data, the `sample::Writer` should have exclusive access to
     * this data during its lifetime. Only afterwards, the `writer` and `refcount` are set by the
     * `sample::Writer`.
     */
    std::atomic<otf2::writer::local*> writer_ = nullptr;
    std::atomic<size_t> ref_count_;
    size_t next_cctx_ref_ = 0;
};
} // namespace lo2s
