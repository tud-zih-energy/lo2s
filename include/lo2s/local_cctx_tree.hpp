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

#include <lo2s/calling_context.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <otf2xx/otf2.hpp>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{

namespace trace
{
class Trace;
}

class LocalCctxTree
{
public:
    LocalCctxTree(trace::Trace& trace, MeasurementScope scope);

    void finalize()
    {
        ref_count_ = next_cctx_ref_;
    }

    void cctx_sample(otf2::chrono::time_point& tp, uint64_t num_ips, const uint64_t ips[]);
    void cctx_sample(otf2::chrono::time_point tp, uint64_t ip);

    template <typename... Cctxs>
    void cctx_enter(const otf2::chrono::time_point& tp, const CallingContext& ctx,
                    const Cctxs&... ctxs)
    {
        cctx_enter(tp, 1, 0, ctx, ctxs...);
    }

    // Enters the nodes ctx...ctxs.
    //
    // - `level` records the level at which `ctx is inserted into the call stack.
    //   "0" is reserved for the CCTX::root node, so the `level` start with 1
    // - `unwind_distance`, which is set by handle_enter_cctx_node, records the
    //   number of newly entered nodes on the call stack
    template <typename... Cctxs>
    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, uint64_t unwind_distance,
                    const CallingContext& ctx, const Cctxs&... ctxs)
    {

        handle_enter_cctx_node(tp, level, unwind_distance, ctx);

        level++;

        cctx_enter(tp, level, unwind_distance, ctxs...);
    }

    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, uint64_t unwind_distance,
                    const CallingContext& cctx)
    {
        handle_enter_cctx_node(tp, level, unwind_distance, cctx);

        writer_.write_calling_context_enter(tp, cur.back()->second.ref, unwind_distance);
    }

    // Leaves all the nodes on the callstack that at an equal or lower level than cctx.
    void cctx_leave(otf2::chrono::time_point tp, uint64_t level, const CallingContext& cctx)
    {
        if (cur.size() <= level)
        {
            if (cur.size() <= 1)
            {
                if (cctx != CallingContext::process(Process(0)))
                {
                    Log::debug() << "Trying to leave " << cctx.name() << " but call stack empty!";
                }
            }
            else
            {
                Log::debug() << "Trying to leave" << cctx.name() << " on level " << level
                             << " of the callstack, but there are only " << cur.size()
                             << " elements on it!";

                if (cur.back()->first != CallingContext::root())
                {
                    writer_.write_calling_context_leave(tp, cur.back()->second.ref);
                }
            }
            return;
        }

        if (cur[level]->first != cctx)
        {
            Log::debug() << "Trying to exit " << cctx.name() << " but " << cur[level]->first.name()
                         << " is on the callstack at level " << level << "!";
            writer_.write_calling_context_leave(tp, cur.back()->second.ref);
            cur.erase(++cur.begin(), cur.end());
        }
        else
        {
            writer_.write_calling_context_leave(tp, cur.back()->second.ref);
            cur.erase(cur.begin() + level, cur.end());
        }
    }

    size_t num_cctx() const
    {
        return ref_count_;
    }

    const LocalCctxMap::value_type& get_tree() const
    {
        return tree;
    }

    otf2::writer::local& writer()
    {
        return writer_;
    }

private:
    // Main code for handling the entering of `cctx` on a specific level.
    // The OTF-2 events are generally produced elsewhere, this is just the
    // logic for manipulating the different lo2s data structures.
    //
    // - `level` is the level at which the new node is entered into the call stack
    // - `unwind_distance` is the number of nodes that have already been entered newly (they were
    // not already on the callstack)
    void handle_enter_cctx_node(otf2::chrono::time_point tp, uint64_t& level,
                                uint64_t& unwind_distance, const CallingContext& cctx)
    {
        // level 0 is the tree root, dont allow inserts besides the root.
        assert(level > 0);

        // If we want to enter a cctx at level x, then the previous 0..x-1
        // calling contexts (recorded in the vector cur) have to be defined
        assert(level <= cur.size());

        if (unwind_distance != 0 || cur.size() == level)
        {
            // If the unwind_distance is already =! 0 or we have reached the end
            // of the current call stack, we are on a definitively on new
            // part of the call stack.
            //
            // in this case, increase the unwind distance, as we are definitively on a new node.
            unwind_distance++;
            cur.emplace_back(create_cctx_node(cctx, cur.back()));
        }
        else
        {
            if (cur.size() > level && cur[level]->first != cctx)
            {
                // If the current callstack (recorded in cur) contains conflicting cctx, first
                // generates a leave event. before entering the new nodes.
                cctx_leave(tp, level, cctx);
                cur.emplace_back(create_cctx_node(cctx, cur.back()));
                unwind_distance = 1;
            }
        }
    }

    LocalCctxMap::value_type* create_cctx_node(const CallingContext& cctx,
                                               LocalCctxMap::value_type* node)
    {
        auto ret =
            node->second.children.emplace(std::piecewise_construct, std::forward_as_tuple(cctx),
                                          std::forward_as_tuple(next_cctx_ref_));

        if (ret.second)
        {
            next_cctx_ref_++;
        }
        return &(*ret.first);
    }

    LocalCctxMap::value_type tree;

    trace::Trace& trace_;
    otf2::writer::local& writer_;
    std::vector<LocalCctxMap::value_type*> cur;
    std::atomic<size_t> ref_count_;
    size_t next_cctx_ref_ = 0;
};
} // namespace lo2s
