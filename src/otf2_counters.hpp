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
#ifndef X86_RECORD_OTF2_OTF2_COUNTER_HPP
#define X86_RECORD_OTF2_OTF2_COUNTER_HPP

#include "otf2_trace.hpp"
#include "perf_counter.hpp"
#include "time.hpp"

#include <otf2xx/writer/local.hpp>

#include <boost/filesystem.hpp>

#include <cstdint>

namespace lo2s
{
class otf2_counters
{
public:
    otf2_counters(pid_t pid, pid_t tid, otf2_trace& trace,
                  otf2::definition::metric_class metric_class, otf2::definition::location scope);

    static otf2::definition::metric_class get_metric_class(otf2_trace& trace);

    void write();

private:
    otf2::writer::local& writer_;
    otf2::definition::metric_instance metric_instance_;
    std::vector<perf_counter> memory_counters_;
    std::vector<otf2::event::metric::value_container> values_;
    boost::filesystem::ifstream proc_stat_;
};
}

#endif // X86_RECORD_OTF2_OTF2_COUNTER_HPP
