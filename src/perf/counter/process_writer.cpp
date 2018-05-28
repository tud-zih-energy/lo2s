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
#include <lo2s/perf/counter/process_writer.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{
ProcessWriter::ProcessWriter(pid_t pid, pid_t tid, otf2::writer::local& writer,
                             monitor::MainMonitor& parent, otf2::definition::location scope,
                             bool enable_on_exec)
: AbstractWriter(
      tid, -1, writer,
      parent.trace().metric_instance(parent.get_metric_class(), writer.location(), scope),
      enable_on_exec),
  proc_stat_(boost::filesystem::path("/proc") / std::to_string(pid) / "task" / std::to_string(tid) /
             "stat")
{
}

void ProcessWriter::handle_custom_events(std::size_t position)
{
    values_[position].set(get_task_last_cpu_id(proc_stat_));
}
} // namespace counter
} // namespace perf
} // namespace lo2s
