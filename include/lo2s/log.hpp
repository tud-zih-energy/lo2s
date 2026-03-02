// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/time/time.hpp>

#include <nitro/log/attribute/message.hpp>
#include <nitro/log/attribute/pid.hpp>
#include <nitro/log/attribute/severity.hpp>
#include <nitro/log/attribute/timestamp.hpp>
#include <nitro/log/filter/severity_filter.hpp>
#include <nitro/log/logger.hpp>
#include <nitro/log/record.hpp>
#include <nitro/log/severity.hpp>
#include <nitro/log/sink/stderr_mt.hpp>

#include <sstream>
#include <string>
#include <utility>

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
        nitro::log::severity_from_string(std::move(verbosity), nitro::log::severity_level::info));
}

} // namespace logging

using Log = logging::Logging;
} // namespace lo2s
