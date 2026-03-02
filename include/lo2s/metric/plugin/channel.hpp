// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/metric/plugin/wrapper.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>
#include <otf2xx/writer/local.hpp>

#include <string>

#include <cstdint>

namespace lo2s::metric::plugin
{
class Channel
{
public:
    Channel(const char* name, const char* description, const char* unit, wrapper::Mode mode,
            wrapper::ValueType value_type, wrapper::ValueBase value_base, std::int64_t exponent,
            trace::Trace& trace);

    const std::string& name() const;

    int& id();

    void write_value(wrapper::TimeValuePair tv);

private:
    int id_{ -1 };
    std::string name_;
    std::string description_;
    std::string unit_;
    wrapper::Mode mode_;
    wrapper::ValueType value_type_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_;
    otf2::event::metric event_;
};
} // namespace lo2s::metric::plugin
