// SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/perf/counter/group/reader.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>

namespace lo2s::perf::counter::group
{
class Writer : public Reader<Writer>
{
public:
    Writer(std::variant<Cpu, Thread> scope, trace::Trace& trace, bool enable_on_exec);

    using Reader<Writer>::handle;
    bool handle(const RecordSampleType* sample);

private:
    time::Converter time_converter_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    otf2::event::metric metric_event_;
};
} // namespace lo2s::perf::counter::group
