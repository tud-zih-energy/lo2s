// SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/perf/counter/userspace/reader.hpp>
#include <lo2s/perf/counter/userspace/userspace_counter_buffer.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>

#include <vector>

namespace lo2s::perf::counter::userspace
{
class Writer : public Reader<Writer>
{
public:
    Writer(std::variant<Cpu, Thread> scope, trace::Trace& trace);

    bool handle(std::vector<UserspaceReadFormat>& data);

private:
    time::Converter time_converter_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric metric_event_;
};
} // namespace lo2s::perf::counter::userspace
