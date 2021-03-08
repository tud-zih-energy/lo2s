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
#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/cpu_set_monitor.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/monitor/process_monitor_main.hpp>
#include <lo2s/summary.hpp>

#include <system_error>

int main(int argc, const char** argv)
{
    try
    {
        lo2s::parse_program_options(argc, argv);
        lo2s::summary();

        switch (lo2s::config().monitor_type)
        {
        case lo2s::MonitorType::CPU_SET:
            lo2s::monitor::CpuSetMonitor().run();
            break;
        case lo2s::MonitorType::PROCESS:
            lo2s::monitor::ProcessMonitor monitor;
            lo2s::monitor::process_monitor_main(monitor);
            break;
        }
    }
    catch (const std::system_error& e)
    {
        // Check for success
        if (e.code())
        {
            lo2s::Log::fatal() << "Aborting: " << e.what();
            return e.code().value();
        }
        else
        {
            lo2s::summary().show();
            return EXIT_SUCCESS;
        }
    }
    catch (const std::exception& e)
    {
        lo2s::Log::fatal() << "Aborting: " << e.what();
        return EXIT_FAILURE;
    }

    return 0;
}
