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

    // Enters the nodes `ctx...ctxs` just above the current node
    template <typename... Cctxs>
    uint64_t cctx_enter(const otf2::chrono::time_point& tp, const CallingContext& ctx,
                        const Cctxs&... ctxs)
    {
        uint64_t level = cur_level() + 1;
        cctx_enter(tp, level, 0, ctx, ctxs...);
        return level;
    }

    // Enters the nodes `ctx...ctxs` on level `level`.
    // If there are already nodes on the cur_ callstack at level, level+1...,
    // level + length(ctx...ctxs), then generate a leave event.
    template <typename... Cctxs>
    uint64_t cctx_enter(const otf2::chrono::time_point& tp, uint64_t level,
                        const CallingContext& ctx, const Cctxs&... ctxs)
    {
        cctx_enter(tp, level, 0, ctx, ctxs...);
        return level;
    }

    uint64_t cctx_enter(const otf2::chrono::time_point& tp, uint64_t level,
                        const CallingContext& ctx)
    {
        cctx_enter(tp, level, 0, ctx);
        return level;
    }

    uint64_t cur_level()
    {
        // The current callstack should always contain the root note!
        assert(cur_.size() > 0);
        return cur_.size() - 1;
    }

    // Leave the top-most node of the callstack
    uint64_t cctx_leave(otf2::chrono::time_point tp)
    {
        return cctx_leave(tp, cur_level());
    }

    // Leaves all the nodes on the callstack that at an equal or lower level than cctx.
    uint64_t cctx_leave(otf2::chrono::time_point tp, uint64_t level)
    {
        // Do not remove root node
        if (level == 0)
        {
            return 1;
        }

        if (cur_level() < level)
        {
            return cur_.size();
        }

        writer_.write_calling_context_leave(tp, cur_.back()->second.ref);

        cur_.erase(cur_.end() - (cur_level() - level + 1), cur_.end());
        return cur_level();
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
    // Enters the nodes ctx...ctxs.
    //
    // - `level`: Level in the callstack at which we insert the new nodes.
    //   level 0 is reserved for the root node, so the first user
    //   inserted node will be at level 1.
    // - `unwind_distance`, the number of nodes we newly entered. OTF-2 needs
    //   to know this number. Tracking the number of newly entered nodes  is handled
    //   by handle_enter_cctx_node.
    template <typename... Cctxs>
    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, uint64_t unwind_distance,
                    const CallingContext& ctx, const Cctxs&... ctxs)
    {

        handle_enter_cctx_node(tp, level, unwind_distance, ctx);

        level++;

        return cctx_enter(tp, level, unwind_distance, ctxs...);
    }

    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, uint64_t unwind_distance,
                    const CallingContext& cctx)
    {
        handle_enter_cctx_node(tp, level, unwind_distance, cctx);

        // Hours spent thinking about the correct usage of unwind_distance: 43 (+-1)
        //
        // Everytime you find yourself thinking about the correct usage of unwind_distance,
        // please increase this number as a warning to others.
        //
        // https://perftools.pages.jsc.fz-juelich.de/cicd/otf2/tags/latest/html/group__records__definition.html#CallingContext
        writer_.write_calling_context_enter(tp, cur_.back()->second.ref, unwind_distance + 1);
    }

    // Function containing the logic that has to be executed on entering every new level
    // in the callstack.
    void handle_enter_cctx_node(otf2::chrono::time_point tp, uint64_t& level,
                                uint64_t& unwind_distance, const CallingContext& cctx)
    {
        // level 0 is the tree root, dont allow inserts besides the root.
        assert(level > 0);

        // If we want to enter a cctx at level x, then the current callstack needs to contain nodes
        // for 1...x-1.
        assert(level - 1 <= cur_level());

        if (unwind_distance != 0 || cur_level() + 1 == level)
        {
            // If the unwind_distance is already =! 0 or we have reached the end
            // of the current call stack, we are on a definitively new
            // part of the call stack.
            //
            // Increase the unwind_distance, because `cctx` is definitly a new node.
            // Also put cctx in to the cctx tree and on the current callstack.
            unwind_distance++;
            cur_.emplace_back(create_cctx_node(cctx, cur_.back()));
        }
        else
        {
            // level is <= cur_level() here
            // check if the node on the current callstack
            // at level `level` is equal to the node we want to insert.
            //
            // if not, leave it (and all nodes further on the callstack)
            if (cur_[level]->first != cctx)
            {
                cctx_leave(tp, level);
                cur_.emplace_back(create_cctx_node(cctx, cur_.back()));
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
    std::vector<LocalCctxMap::value_type*> cur_;
    std::atomic<size_t> ref_count_;
    size_t next_cctx_ref_ = 0;
};
} // namespace lo2s
