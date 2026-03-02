// SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/execution_scope.hpp>
#include <lo2s/local_cctx_tree.hpp>
#include <lo2s/perf/syscall/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/chrono/time_point.hpp>

namespace lo2s::perf::syscall
{
// Note, this cannot be protected for CRTP reasons...
class Writer : public Reader<Writer>
{
public:
    Writer(ExecutionScope scope, trace::Trace& trace);

    Writer(const Writer& other) = delete;

    Writer& operator=(Writer&) = delete;
    Writer& operator=(Writer&&) = delete;

    Writer(Writer&& other) noexcept = default;

    ~Writer();

    using Reader<Writer>::handle;

    bool handle(const Reader::RecordSampleType* sample);

private:
    static constexpr int CCTX_LEVEL_SYSCALL = 1;

    LocalCctxTree& local_cctx_tree_;
    const time::Converter& time_converter_;

    otf2::chrono::time_point last_tp_;
};
} // namespace lo2s::perf::syscall
