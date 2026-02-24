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
#include <lo2s/measurement_scope.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/chrono/time_point.hpp>
#include <otf2xx/writer/local.hpp>

#include <atomic>
#include <tuple>
#include <utility>
#include <vector>

#include <cassert>
#include <cstddef>
#include <cstdint>

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

    // All cctx_enter variants return the level of the first new node on the call stack.
    // This allows for constructs such as:
    //
    // to_exit = cctx_enter(...);
    // cctx_exit(to_exit)
    //
    // To easily exit the nodes entered on a previous cctx_enter call.

    // Enters the nodes ctx...ctxs below the current call stack
    template <typename... Cctxs>
    uint64_t cctx_enter(const otf2::chrono::time_point& tp, const CallingContext& ctx,
                        const Cctxs&... ctxs)
    {
        const uint64_t level = cur_level() + 1;
        cctx_enter(tp, level, ctx, ctxs...);
        return level;
    }

    // Enters the new nodes ctx...ctxs recursively.
    //
    // - level: Level at which the new nodes are entered on the  current callstack.
    //   level 0 is reserved for the root node, so the first user
    //   inserted node will be at level 1.
    template <typename... Cctxs>
    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, const CallingContext& ctx,
                    const Cctxs&... ctxs)
    {

        handle_enter_cctx_node(tp, level, ctx);

        level++;

        return cctx_enter(tp, level, ctxs...);
    }

    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, const CallingContext& cctx)
    {
        handle_enter_cctx_node(tp, level, cctx);
    }

    uint64_t cur_level()
    {
        // The current callstack should always contain the root note
        assert(cur_.size() > 0);
        return cur_.size() - 1;
    }

    // Leave the top-most node of the callstack
    uint64_t cctx_leave(otf2::chrono::time_point tp)
    {
        return cctx_leave(tp, cur_level());
    }

    // Leaves all the nodes on the callstack that are at the level `level` or below.
    uint64_t cctx_leave(otf2::chrono::time_point tp, uint64_t level)
    {
        // Do not remove root node.
        // Also, if we are asked to leave a node on level X but the current
        // callstack is not that deep, do nothing.
        while (level > 0 && cur_level() >= level)
        {
            writer_.write_calling_context_leave(tp, cur_.back()->second.ref);
            cur_.pop_back();
        }

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
    // The magic that has to happen for every entered node, happens here.
    void handle_enter_cctx_node(otf2::chrono::time_point tp, uint64_t& level,
                                const CallingContext& cctx)
    {
        // level 0 is the tree root, dont allow inserts besides the root.
        assert(level > 0);

        // If we want to enter a cctx at level x, then the current callstack needs to contain nodes
        // on level 0 until x-1.
        assert(level <= cur_level() + 1);

        if (level == cur_level() + 1)
        {
            // We are on a definitely new part of the callstack.
            cur_.emplace_back(create_cctx_node(cctx, cur_.back()));
            writer_.write_calling_context_enter(tp, cur_.back()->second.ref, 2);
        }
        else
        {
            // level is <= cur_level() here, so there is already
            // a node on the current callstack at the level we want to insert
            // the new one.
            //
            // If the current and new node match, do nothing, the callstack
            // has not changed on that level.
            //
            // Otherwise, leave all the nodes on the current callstack
            // beginning with the mismatched node and enter the new one.
            if (cur_[level]->first != cctx)
            {
                cctx_leave(tp, level);
                cur_.emplace_back(create_cctx_node(cctx, cur_.back()));
                writer_.write_calling_context_enter(tp, cur_.back()->second.ref, 2);
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
