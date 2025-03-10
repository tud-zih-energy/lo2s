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
#include <lo2s/measurement_scope.hpp>
#include <otf2xx/otf2.hpp>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{

enum class CallingContextType
{
    ROOT,
    PROCESS,
    THREAD,
    SAMPLE_ADDR
};

// Type that encodes all the different types of CallingContext we can have
struct CallingContext
{
public:
    static CallingContext process(Process p)
    {
        CallingContext c;
        c.type = CallingContextType::PROCESS;
        c.p = p;
        return c;
    }

    static CallingContext thread(Thread t)
    {
        CallingContext c;
        c.type = CallingContextType::THREAD;
        c.t = t;
        return c;
    }

    static CallingContext sample(Address addr)
    {
        CallingContext c;
        c.type = CallingContextType::SAMPLE_ADDR;
        c.addr = addr;
        return c;
    }

    static CallingContext root()
    {
        CallingContext c;
        c.type = CallingContextType::ROOT;
        return c;
    }

    Address to_addr() const
    {
        if (type == CallingContextType::SAMPLE_ADDR)
        {
            return addr;
        }

        throw std::runtime_error("Not an Address!");
    }

    Process to_process() const
    {
        if (type == CallingContextType::PROCESS)
        {
            return p;
        }

        throw std::runtime_error("Not a Process!");
    }

    Thread to_thread() const
    {
        if (type == CallingContextType::THREAD)
        {
            return t;
        }

        throw std::runtime_error("Not a Process!");
    }

    CallingContextType type;

    friend bool operator<(const CallingContext& lhs, const CallingContext& rhs)
    {
        if (lhs.type != rhs.type)
        {
            return lhs.type < rhs.type;
        }
        else
        {
            switch (lhs.type)
            {
            case CallingContextType::PROCESS:
                return lhs.p < rhs.p;
            case CallingContextType::THREAD:
                return lhs.t < rhs.t;
            case CallingContextType::SAMPLE_ADDR:
                return lhs.addr < rhs.addr;
            case CallingContextType::ROOT:
                throw std::runtime_error("Can not have two CallingContext Roots!");
            default:
                throw std::runtime_error("Unknown Cctx Type!");
            }
        }
    }

    friend bool operator==(const CallingContext& lhs, const CallingContext& rhs)
    {
        if (lhs.type == rhs.type)
        {
            switch (lhs.type)
            {
            case CallingContextType::ROOT:
                return true;
            case CallingContextType::PROCESS:
                return lhs.p == rhs.p;
            case CallingContextType::THREAD:
                return lhs.t == rhs.t;
            case CallingContextType::SAMPLE_ADDR:
                return lhs.addr == rhs.addr;
            default:
                return false;
            }
        }
        else
        {
            return false;
        }
    }

private:
    CallingContext() : addr(0)
    {
    }

    Process p;
    Thread t;
    Address addr;
};

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
    LocalCctxTree() : tree(CallingContext::root(), LocalCctxNode(0, nullptr)), cur(&tree)
    {
    }

    void finalize(otf2::writer::local* otf2_writer)
    {
        ref_count_ = next_cctx_ref_;
        // set writer last, because it is used as sentry to confirm that the cctx refs are properly
        // finalized.
        writer_ = otf2_writer;
    }

    otf2::definition::calling_context::reference_type sample_ref(uint64_t num_ips,
                                                                 const uint64_t ips[])
    {
        auto* node = cur;

        for (uint64_t i = num_ips - 1; i != 0; i--)
        {
            if (ips[i] == PERF_CONTEXT_KERNEL)
            {
                if (i <= 1)
                {
                    return node->second.ref;
                }
                continue;
            }
            else if (ips[i] == PERF_CONTEXT_USER)
            {
                if (i <= 1)
                {
                    return node->second.ref;
                }
                continue;
            }
            node = create_cctx_node(CallingContext::sample(ips[i]), node);
        }

        return node->second.ref;
    }

    otf2::definition::calling_context::reference_type sample_ref(uint64_t ip)
    {
        return create_cctx_node(CallingContext::sample(ip), cur)->second.ref;
    }

    otf2::definition::calling_context::reference_type cctx_enter(CallingContext cctx)
    {
        cur = create_cctx_node(cctx, cur);
        return cur->second.ref;
    }

    otf2::definition::calling_context::reference_type cctx_exit()
    {
        if (cur->first.type == CallingContextType::ROOT)
        {
            throw std::runtime_error("Trying to exit root calling context!");
        }

        auto ref = cur->second.ref;
        cur = cur->second.parent;
        return ref;
    }

    bool is_current(const CallingContext cctx) const
    {
        return cur->first == cctx;
    }

    const CallingContext& cur_cctx() const
    {
        return cur->first;
    }

    size_t num_cctx() const
    {
        return ref_count_;
    }

    const LocalCctxMap::value_type& get_tree() const
    {
        return tree;
    }

    otf2::writer::local* writer()
    {
        return writer_;
    }

private:
    LocalCctxMap::value_type* create_cctx_node(CallingContext cctx, LocalCctxMap::value_type* node)
    {
        auto ret =
            node->second.children.emplace(std::piecewise_construct, std::forward_as_tuple(cctx),
                                          std::forward_as_tuple(next_cctx_ref_, cur));

        if (ret.second)
        {
            next_cctx_ref_++;
        }
        return &(*ret.first);
    }

    LocalCctxMap::value_type tree;
    LocalCctxMap::value_type* cur;

    /*
     * Records the local calling contexts encountered in a Writer.
     * While the `Trace` always owns this data, the `Writer` should have exclusive access to
     * this data during its lifetime. Only afterwards, the `writer` and `refcount` are set by the
     * `Writer`, and control of this data is handed over to the Trace.
     */
    std::atomic<otf2::writer::local*> writer_ = nullptr;
    std::atomic<size_t> ref_count_;
    size_t next_cctx_ref_ = 0;
};
} // namespace lo2s
