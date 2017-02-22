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

#include <nitro/log/log.hpp>

#include <nitro/log/sink/stderr_mt.hpp>

#include <nitro/log/attribute/message.hpp>
#include <nitro/log/attribute/pid.hpp>
#include <nitro/log/attribute/pthread_id.hpp>
#include <nitro/log/attribute/severity.hpp>
#include <nitro/log/attribute/timestamp.hpp>

#include <nitro/log/filter/and_filter.hpp>
#include <nitro/log/filter/mpi_master_filter.hpp>
#include <nitro/log/filter/severity_filter.hpp>

#include <fstream>
#include <mutex>

namespace lo2s
{
namespace logging
{

    using Record =
        nitro::log::record<nitro::log::message_attribute, nitro::log::timestamp_attribute,
                           nitro::log::severity_attribute, nitro::log::pid_attribute,
                           nitro::log::pthread_id_attribute>;

    template <typename R>
    class Lo2sLogFormatter
    {
    public:
        std::string format(R& r)
        {
            std::stringstream s;

            s << "[" << r.timestamp().count() << "][pid: " << r.pid() << "][tid:" << r.pthread_id()
              << "][" << r.severity() << "]: " << r.message();

            return s.str();
        }
    };

    template <typename R>
    using Lo2sFilter = nitro::log::filter::severity_filter<R>;

    inline std::ostream& i_hate_init()
    {
        static std::ofstream fileout("output.txt");

        return fileout;
    }

    template <bool Foo = false>
    struct Fileout
    {
        static std::mutex stdout_mutex;

        void sink(std::string formatted_record)
        {
            std::lock_guard<std::mutex> my_lock(stdout_mutex);

            i_hate_init() << formatted_record << std::endl;
        }
    };

    template <bool Foo>
    std::mutex Fileout<Foo>::stdout_mutex;

    using Logging = nitro::log::logger<Record, Lo2sLogFormatter, nitro::log::sink::stderr_mt,
                                       Lo2sFilter>;

    inline void set_min_severity_level(nitro::log::severity_level sev)
    {
        Lo2sFilter<Record>::set_severity(sev);
    }

    inline void set_min_severity_level(std::string verbosity)
    {
        set_min_severity_level(
            nitro::log::severity_from_string(verbosity, nitro::log::severity_level::info));
    }

} // namespace logging

using Log = logging::Logging;
}
