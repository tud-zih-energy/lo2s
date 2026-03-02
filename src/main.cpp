// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include <lo2s/config.hpp>
#include <lo2s/config/monitor_type.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/cpu_set_monitor.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/monitor/process_monitor_main.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/util.hpp>

#include <exception>
#include <system_error>

#include <cstdlib>

int main(int argc, const char** argv)
{
    // The resource limit for file descriptors (which lo2s uses a lot of, especially in
    // system-monitoring mode) is artifically low to cope with the ancient select() systemcall. We
    // do not use select(), so we can safely bump the limit, but whatever command we are running
    // under lo2s might (and resource limits are preserved accross fork()) so preserve it here so
    // that we can restore it later
    lo2s::initial_rlimit_fd();
    lo2s::bump_rlimit_fd();

    try
    {
        lo2s::parse_program_options(argc, argv);
        lo2s::summary();

        switch (lo2s::config().general.monitor_type)
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
    }
    catch (const std::exception& e)
    {
        lo2s::Log::fatal() << "Aborting: " << e.what();
        return EXIT_FAILURE;
    }

    lo2s::summary().show();

    return 0;
}
