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

#include <lo2s/time/time.hpp>

#include <nitro/log/attribute/message.hpp>
#include <nitro/log/attribute/pid.hpp>
#include <nitro/log/attribute/severity.hpp>
#include <nitro/log/attribute/timestamp.hpp>
#include <nitro/log/filter/and_filter.hpp>
#include <nitro/log/filter/mpi_master_filter.hpp>
#include <nitro/log/filter/severity_filter.hpp>
#include <nitro/log/log.hpp>
#include <nitro/log/sink/stderr_mt.hpp>

#include <fstream>
#include <mutex>

namespace lo2s
{
namespace logging
{

using Record = nitro::log::record<nitro::log::message_attribute,
                                  nitro::log::timestamp_clock_attribute<lo2s::time::Clock>,
                                  nitro::log::severity_attribute, nitro::log::pid_attribute>;

template <typename R>
class Lo2sLogFormatter
{
public:
    std::string format(R& r)
    {
        std::stringstream s;

        s << "[" << r.timestamp().time_since_epoch().count() << "][pid: " << r.pid()
          << "][tid: " << r.tid() << "][" << r.severity() << "]: " << r.message() << '\n';

        return s.str();
    }
};

template <typename R>
using Lo2sFilter = nitro::log::filter::severity_filter<R>;

using Logging =
    nitro::log::logger<Record, Lo2sLogFormatter, nitro::log::sink::StdErrThreaded, Lo2sFilter>;

inline void set_min_severity_level(nitro::log::severity_level sev)
{
    Lo2sFilter<Record>::set_severity(sev);
}

inline nitro::log::severity_level get_min_severity_level()
{
    return Lo2sFilter<Record>::min_severity();
}

inline void set_min_severity_level(std::string verbosity)
{
    set_min_severity_level(
        nitro::log::severity_from_string(verbosity, nitro::log::severity_level::info));
}

} // namespace logging

using Log = logging::Logging;
} // namespace lo2s
