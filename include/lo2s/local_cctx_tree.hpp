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

// Node type of the tree containing the CallingContext -> local cctx reference number mappings.
struct LocalCctxNode
{
    LocalCctxNode(otf2::definition::calling_context::reference_type r,
                  std::map<CallingContext, LocalCctxNode>::value_type* parent)
    : ref(r), parent(parent)
    {
    }

    otf2::definition::calling_context::reference_type ref;
    std::map<CallingContext, LocalCctxNode> children;
    std::map<CallingContext, LocalCctxNode>::value_type* parent;
};

// Node type of the global cctx tree, containing CallingContext -> otf2::cctx mappings.
struct GlobalCctxNode
{
    GlobalCctxNode(otf2::definition::calling_context* cctx) : cctx(cctx)
    {
    }

    otf2::definition::calling_context* cctx;
    std::map<CallingContext, GlobalCctxNode> children;
};

typedef std::map<CallingContext, LocalCctxNode> LocalCctxMap;
typedef std::map<CallingContext, GlobalCctxNode> GlobalCctxMap;

class LocalCctxTree
{
public:
    LocalCctxTree(trace::Trace& trace, otf2::writer::local& writer);

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

    template <typename... Cctxs>
    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, uint64_t unwind_distance,
                    const CallingContext& ctx, const Cctxs&... ctxs)
    {
        if (unwind_distance != 0)
        {
            unwind_distance++;
        }

        if (cur.size() > level && cur[level]->first != ctx)
        {
            cctx_leave(tp, ctx);
            unwind_distance = 1;
        }

        if (cur.size() <= level)
        {
            cur.emplace_back(create_cctx_node(ctx, cur.back()));
        }
        level++;
        cctx_enter(tp, level, unwind_distance, ctxs...);
    }

    void cctx_enter(const otf2::chrono::time_point& tp, uint64_t level, uint64_t unwind_distance,
                    const CallingContext& cctx);

    void cctx_leave(otf2::chrono::time_point tp, const CallingContext& cctx)
    {
        if (cur.size() == 1)
        {
            Log::debug() << "Trying to exit empty cctx stack!";
            return;
        }

        auto it =
            std::find_if(cur.begin(), cur.end(), [&](auto elem) { return elem->first == cctx; });
        if (it == cur.end())
        {
            Log::debug() << "Trying to exit " << cctx.name() << " but it is not on the callstack!";
            writer_.write_calling_context_leave(tp, cur.back()->second.ref);
            cur.erase(++cur.begin(), cur.end());
        }
        else
        {
            writer_.write_calling_context_leave(tp, cur.back()->second.ref);
            cur.erase(it, cur.end());
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
    LocalCctxMap::value_type* create_cctx_node(const CallingContext& cctx,
                                               LocalCctxMap::value_type* node)
    {
        auto ret =
            node->second.children.emplace(std::piecewise_construct, std::forward_as_tuple(cctx),
                                          std::forward_as_tuple(next_cctx_ref_, cur.back()));

        if (ret.second)
        {
            next_cctx_ref_++;
        }
        return &(*ret.first);
    }

    LocalCctxMap::value_type tree;

    /*
     * Records the local calling contexts encountered in a Writer.
     * While the `Trace` always owns this data, the `Writer` should have exclusive access to
     * this data during its lifetime. Only afterwards, the `writer` and `refcount` are set by the
     * `Writer`, and control of this data is handed over to the Trace.
     */
    trace::Trace& trace_;
    otf2::writer::local& writer_;
    std::vector<LocalCctxMap::value_type*> cur;
    std::atomic<size_t> ref_count_;
    size_t next_cctx_ref_ = 0;
};
} // namespace lo2s
