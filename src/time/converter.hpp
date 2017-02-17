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

#ifndef LO2S_TIME_CONVERTER_HPP
#define LO2S_TIME_CONVERTER_HPP

#include "time.hpp"
#include "reader.hpp"

#include <otf2xx/chrono/chrono.hpp>

#include <cstdint>

namespace lo2s
{
class perf_time_converter
{
public:
    perf_time_converter();

    otf2::chrono::time_point operator()(std::uint64_t perf_raw) const
    {
        return operator()(perf_tp(perf_raw));
    }

    otf2::chrono::time_point operator()(perf_clock::time_point perf_tp) const
    {
        return otf2::chrono::time_point(perf_tp.time_since_epoch() + offset);
    }

    perf_clock::time_point operator()(otf2::chrono::time_point local_tp) const
    {
        return perf_clock::time_point(local_tp.time_since_epoch() - offset);
    }

private:
    otf2::chrono::duration offset;
};
}
#endif // LO2S_TIME_CONVERTER_HPP
