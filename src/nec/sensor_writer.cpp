/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <lo2s/nec/sensor_writer.hpp>
#include <lo2s/trace/trace.hpp>

#include <filesystem>
#include <regex>

namespace lo2s
{
namespace nec
{

SensorWriter::SensorWriter(trace::Trace& trace, const Device& device)
: writer_(trace.nec_sensor_writer(device)),
  metric_instance_(trace.metric_instance(trace.nec_metric_class(device), writer_.location(),
                                         writer_.location())),
  metric_event_(otf2::chrono::genesis(), metric_instance_), device_(device)
{
}

void SensorWriter::write()
{
    metric_event_.timestamp(lo2s::time::now());

    otf2::event::metric::values& values = metric_event_.raw_values();

    int i = 0;
    for (const auto& sensor : device_.sensors())
    {
        values[i] = sensor.read();
    }

    writer_.write(metric_event_);
}
} // namespace nec
} // namespace lo2s
