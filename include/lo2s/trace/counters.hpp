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

#include <lo2s/trace/trace.hpp>

#include <lo2s/metric/perf_counter.hpp>
#include <lo2s/time/time.hpp>

#include <otf2xx/writer/local.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <cstdint>

namespace lo2s
{
namespace trace
{
class Counters
{
public:
    Counters(pid_t pid, pid_t tid, Trace& trace, otf2::definition::metric_class metric_class,
             otf2::definition::location scope);

    static otf2::definition::metric_class get_metric_class(Trace& trace);

    void write();

private:
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    // XXX this should depend here!
    std::vector<metric::PerfCounter> counters_;
    std::vector<otf2::event::metric::value_container> values_;
    boost::filesystem::ifstream proc_stat_;
};
}
}
