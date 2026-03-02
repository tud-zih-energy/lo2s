// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/local_cctx_tree.hpp>

#include <lo2s/calling_context.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/chrono/time_point.hpp>

#include <cstdint>

#include <linux/perf_event.h>

namespace lo2s
{
LocalCctxTree::LocalCctxTree(trace::Trace& trace, MeasurementScope scope)
: tree(CallingContext::root(), LocalCctxNode(0)), trace_(trace),
  writer_(trace_.sample_writer(scope)), cur_({ &tree })
{
}

void LocalCctxTree::cctx_sample(otf2::chrono::time_point& tp, uint64_t num_ips,
                                const uint64_t ips[])
{
    auto* node = cur_.back();

    for (uint64_t i = num_ips - 1; i != 0; i--)
    {
        if (ips[i] == PERF_CONTEXT_KERNEL || ips[i] == PERF_CONTEXT_USER)
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
    auto* node = create_cctx_node(CallingContext::sample(ip), cur_.back());
    writer_.write_calling_context_sample(tp, node->second.ref, 2,
                                         trace_.interrupt_generator().ref());
}

} // namespace lo2s
