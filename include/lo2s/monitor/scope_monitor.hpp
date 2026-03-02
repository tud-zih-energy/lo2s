// Copyright (C)  Technische Universitaet Dresden, Germany
// SPDX-FileCopyrightText: 2016-2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/execution_scope.hpp>
#include <lo2s/monitor/fwd.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/counter/group/writer.hpp>
#include <lo2s/perf/counter/userspace/writer.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/perf/syscall/writer.hpp>
#include <lo2s/resolvers.hpp>
#include <lo2s/trace/fwd.hpp>

#include <memory>
#include <string>

namespace lo2s::monitor
{

class ScopeMonitor : public PollMonitor
{
public:
    ScopeMonitor(ExecutionScope scope, trace::Trace& trace, bool enable_on_exec);

    void initialize_thread() override;
    void finalize_thread() override;
    void monitor(int fd) override;

    std::string group() const override
    {
        if (scope_.is_cpu())
        {
            return "lo2s::CpuMonitor";
        }
        return "lo2s::ThreadMonitor";
    }

    void emplace_resolvers(Resolvers& resolvers)
    {
        if (sample_writer_)
        {
            sample_writer_->emplace_resolvers(resolvers);
        }
    }

private:
    ExecutionScope scope_;
    std::unique_ptr<perf::syscall::Writer> syscall_writer_;
    std::unique_ptr<perf::sample::Writer> sample_writer_;
    std::unique_ptr<perf::counter::group::Writer> group_counter_writer_;
    std::unique_ptr<perf::counter::userspace::Writer> userspace_counter_writer_;
};
} // namespace lo2s::monitor
