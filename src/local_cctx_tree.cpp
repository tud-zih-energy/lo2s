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

#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
LocalCctxTree::LocalCctxTree(trace::Trace& trace, otf2::writer::local& writer)
: tree(CallingContext::root(), LocalCctxNode(0, nullptr)), trace_(trace), writer_(writer),
  cur({ &tree })
{
}

void LocalCctxTree::cctx_sample(otf2::chrono::time_point& tp, uint64_t num_ips,
                                const uint64_t ips[])
{
    auto* node = cur.back();

    for (uint64_t i = num_ips - 1; i != 0; i--)
    {
        if (ips[i] == PERF_CONTEXT_KERNEL)
        {
            if (i <= 1)
            {
                break;
            }
            continue;
        }
        else if (ips[i] == PERF_CONTEXT_USER)
        {
            if (i <= 1)
            {
                break;
            }
            continue;
        }
        node = create_cctx_node(CallingContext::sample(ips[i]), node);
    }

    writer_.write_calling_context_sample(tp, node->second.ref, num_ips,
                                         trace_.interrupt_generator().ref());
}

void LocalCctxTree::cctx_sample(otf2::chrono::time_point tp, uint64_t ip)
{
    auto* node = create_cctx_node(CallingContext::sample(ip), cur.back());
    writer_.write_calling_context_sample(tp, node->second.ref, 2,
                                         trace_.interrupt_generator().ref());
}

void LocalCctxTree::cctx_enter(const otf2::chrono::time_point& tp, uint64_t level,
                               uint64_t unwind_distance, const CallingContext& cctx)
{

    if (unwind_distance != 0)
    {
        unwind_distance++;
    }

    if (cur.size() > level && cur[level]->first != cctx)
    {
        cctx_leave(tp, cctx);
        unwind_distance = 1;
    }

    if (cur.size() <= level)
    {
        cur.emplace_back(create_cctx_node(cctx, cur.back()));
    }

    writer_.write_calling_context_enter(tp, cur.back()->second.ref, unwind_distance);
}

} // namespace lo2s
