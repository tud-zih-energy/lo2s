/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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
#include <lo2s/monitor/main_monitor.hpp>
#include <lo2s/perf/counter/abstract_writer.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{

class CpuWriter : public AbstractWriter
{
public:
    CpuWriter(int cpuid, otf2::writer::local& writer, monitor::MainMonitor& parent)
    : AbstractWriter(-1, cpuid, writer,
                     parent.trace().metric_instance(parent.get_metric_class(), writer.location(),
                                                    parent.trace().cpu_writer(cpuid).location()),
                     false)
    {
    }

private:
    void handle_custom_events(std::size_t /*position*/)
    {
    }
};
} // namespace counter
} // namespace perf
} // namespace lo2s
