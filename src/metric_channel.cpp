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
#include "metric_channel.hpp"

namespace lo2s
{

static otf2::common::type convert_type(metric_plugin_wrapper::ValueType value_type)
{
    switch (value_type)
    {
    case metric_plugin_wrapper::ValueType::INT64:
        return otf2::common::type::int64;
    case metric_plugin_wrapper::ValueType::UINT64:
        return otf2::common::type::uint64;
    case metric_plugin_wrapper::ValueType::DOUBLE:
        return otf2::common::type::Double;
    default:
        throw std::runtime_error("Unexpected value type given");
    }
}

metric_channel::metric_channel(const char* name, const char* description, const char* unit,
                               metric_plugin_wrapper::Mode mode,
                               metric_plugin_wrapper::ValueType value_type, otf2_trace& trace)
: id_(-1), name_(name), description_(), unit_(), mode_(mode), value_type_(value_type),
  writer_(trace.metric_writer(name_)),
  metric_(trace.metric_instance(trace.metric_class(), writer_.location(),
                                writer_.location().location_group().parent())),
  values_(1)
{
    if (description != nullptr)
    {
        description_ = description;
    }

    if (unit != nullptr)
    {
        unit_ = unit;
    }

    auto mc = otf2::definition::make_weak_ref(metric_.metric_class());

    mc->add_member(
        trace.metric_member(name_, description_, static_cast<otf2::common::metric_mode>(mode_),
                            convert_type(value_type_), unit_));

    values_[0].metric = (*mc)[0];
}

const std::string& metric_channel::name() const
{
    return name_;
}

int& metric_channel::id()
{
    return id_;
}

void metric_channel::write_value(metric_plugin_wrapper::time_value_pair tv)
{
    values_[0].value.unsigned_int = tv.value;

    writer_.write(otf2::event::metric(
        otf2::chrono::time_point(otf2::chrono::duration(tv.timestamp)), metric_, values_));
}
}
