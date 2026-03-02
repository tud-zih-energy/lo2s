// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/perf/time/converter.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/perf/tracepoint/reader.hpp>
#include <lo2s/trace/fwd.hpp>
#include <lo2s/types/cpu.hpp>

#include <otf2xx/definition/metric_class.hpp>
#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

namespace lo2s::perf::tracepoint
{
// Note, this cannot be protected for CRTP reasons...
class Writer : public Reader<Writer>
{
public:
    Writer(Cpu cpu, const perf::tracepoint::TracepointEventAttr& event, trace::Trace& trace,
           const otf2::definition::metric_class& metric_class);

    Writer(const Writer& other) = delete;
    Writer& operator=(Writer&) = delete;

    Writer& operator=(Writer&&) noexcept = delete;
    Writer(Writer&& other) noexcept = default;

    ~Writer() = default;

    using Reader<Writer>::handle;

    bool handle(const Reader::RecordSampleType* sample);

private:
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;

    const time::Converter time_converter_;

    otf2::event::metric metric_event_;
};
} // namespace lo2s::perf::tracepoint
