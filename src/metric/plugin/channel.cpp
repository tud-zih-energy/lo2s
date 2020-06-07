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
#include <lo2s/metric/plugin/channel.hpp>

#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/definitions.hpp>

#include <memory>
#include <stdexcept>

namespace lo2s
{
namespace metric
{
namespace plugin
{

static otf2::definition::metric_instance&&
make_metric_instance(otf2::definition::metric_instance&& instance,
                     otf2::definition::metric_member&& member)
{
    auto mc = otf2::definition::make_weak_ref(instance.metric_class());
    mc->add_member(std::move(member));
    return std::move(instance);
}

static const char* empty_if_null(const char* cstr)
{
    return cstr == nullptr ? "" : cstr;
}

Channel::Channel(const wrapper::Properties& event, trace::Trace& trace)
: id_(-1), name_(event.name), writer_(trace.named_metric_writer(name_)),
  metric_(make_metric_instance(trace.metric_instance(trace.metric_class(), writer_.location(),
                                                     writer_.location().location_group().parent()),
                               trace.metric_member(name_, empty_if_null(event.description),
                                                   wrapper::convert_mode(event.mode),
                                                   wrapper::convert_type(event.value_type),
                                                   empty_if_null(event.unit), event.exponent,
                                                   wrapper::convert_base(event.base)))),
  event_(otf2::chrono::genesis(), metric_)
{
}

const std::string& Channel::name() const
{
    return name_;
}

void Channel::id(int id)
{
    id_ = id;
}

int Channel::id() const
{
    return id_;
}

bool Channel::isDisabled() const
{
    return id_ == -1;
}

void Channel::write_values(wrapper::TimeValuePair* begin, wrapper::TimeValuePair* end,
                           otf2::chrono::time_point from, otf2::chrono::time_point to)
{
    for (auto it = begin; it != end; it++)
    {
        auto timestamp = otf2::chrono::time_point(otf2::chrono::duration(it->timestamp));
        if (from <= timestamp && timestamp <= to)
        {
            write_value(*it);
        }
    }
}

void Channel::write_value(wrapper::TimeValuePair tv)
{
    // @tilsche look behind you, a three-headed monkey! -- This is necessary, because we forced too
    // much type-safety in the metric event refactoring :( Need to change that. Band-aid incoming.
    const_cast<std::vector<OTF2_MetricValue>&>(event_.raw_values().values())[0].unsigned_int =
        tv.value;
    event_.timestamp(otf2::chrono::time_point(otf2::chrono::duration(tv.timestamp)));
    writer_.write(event_);
}
} // namespace plugin
} // namespace metric
} // namespace lo2s
