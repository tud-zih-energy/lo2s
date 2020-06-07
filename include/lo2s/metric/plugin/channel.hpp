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

#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/fwd.hpp>
#include <otf2xx/definition/metric_instance.hpp>
#include <otf2xx/event/metric.hpp>

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
    Channel(const wrapper::Properties& event, trace::Trace& trace);

public:
    const std::string& name() const;

    bool isDisabled() const;

    void id(int id);
    int id() const;

    void write_values(wrapper::TimeValuePair* begin, wrapper::TimeValuePair* end,
                      otf2::chrono::time_point from, otf2::chrono::time_point to);
    void write_value(wrapper::TimeValuePair tv);

private:
    int id_;
    std::string name_;
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_;
    otf2::event::metric event_;
};
} // namespace plugin
} // namespace metric
} // namespace lo2s
