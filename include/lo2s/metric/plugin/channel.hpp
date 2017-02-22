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

#include <lo2s/metric/plugin/wrapper.hpp>

#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/fwd.hpp>
#include <otf2xx/definition/metric_instance.hpp>

#include <memory>
#include <string>
#include <vector>

namespace lo2s
{
namespace metric
{
namespace plugin
{
class Channel
{
public:
    Channel(const char* name, const char* description, const char* unit, wrapper::Mode mode,
            wrapper::ValueType value_type, trace::Trace& trace);

public:
    const std::string& name() const;

    int& id();

    void write_value(wrapper::TimeValuePair tv);

private:
    int id_;
    std::string name_;
    std::string description_;
    std::string unit_;
    wrapper::Mode mode_;
    wrapper::ValueType value_type_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_;
    std::vector<otf2::event::metric::value_container> values_;
};
}
}
}
