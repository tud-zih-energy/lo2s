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

#ifndef LO2S_TIME_READER_HPP
#define LO2S_TIME_READER_HPP

#include "../log.hpp"

#include "../perf_sample_reader.hpp"
#include "time.hpp"

#include <otf2xx/chrono/chrono.hpp>

namespace lo2s
{
class time_reader : public perf_sample_reader<time_reader>
{
public:
    time_reader();

public:
    using perf_sample_reader<time_reader>::handle;
#ifndef HW_BREAKPOINT_COMPAT
    using record_sync_type = perf_sample_reader::record_sample_type;
#else
    using record_sync_type = perf_sample_reader::record_fork_type;
#endif
    bool handle(const record_sync_type* sync_event);


public:
    otf2::chrono::time_point local_time = otf2::chrono::genesis();
    perf_clock::time_point perf_time;
};
}
#endif // LO2S_TIME_READER_HPP
