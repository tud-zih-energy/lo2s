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

#include <lo2s/monitor/process_monitor_main.hpp>

#include <lo2s/monitor/abstract_process_monitor.hpp>

#include <lo2s/process_controller.hpp>
#include <lo2s/util.hpp>

#include <lo2s/config.hpp>
#include <lo2s/error.hpp>
#include <lo2s/log.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <cassert>

extern "C"
{
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_LINUX
#include <sys/prctl.h>
#endif
#include <sys/ptrace.h>
}

namespace lo2s
{
namespace monitor
{

static void run_command(const std::vector<std::string>& command_and_args)
{
#ifdef HAVE_LINUX
    /* kill yourself if the parent dies */
    prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif

#ifdef HAVE_LINUX
    /* we need ptrace to get fork/clone/... */
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
#endif
#ifdef HAVE_DARWIN
    /* we need ptrace to get fork/clone/... */
    ptrace(PT_TRACE_ME, 0, NULL, 0);
#endif

    std::vector<char*> tmp;
    std::transform(command_and_args.begin(), command_and_args.end(), std::back_inserter(tmp),
                   [](const std::string& s) {
                       char* pc = new char[s.size() + 1];
                       std::strcpy(pc, s.c_str());
                       return pc;
                   });
    tmp.push_back(nullptr);

    Log::debug() << "execve(" << command_and_args.at(0) << ")";

    // Stop yourself so the parent tracer can do initialize the options
    raise(SIGSTOP);

    // run the application which should be sampled
    execvp(tmp[0], &tmp[0]);

    // should not be executed -> exec failed, let's clean up anyway.
    for (auto cp : tmp)
    {
        delete[] cp;
    }
    Log::error() << "Could not execute command: " << command_and_args.at(0);
    exit(errno);
}

void process_monitor_main(AbstractProcessMonitor& monitor)
{

    auto pid = config().pid;
    assert(pid != 0);
    bool spawn = (pid == -1);
    if (spawn)
    {
        pid = fork();
    }
    else
    {
        // TODO Attach to all threads in a process
        int res;

#ifdef HAVE_LINUX
        res = ptrace(PTRACE_ATTACH, pid, nullptr, nullptr);
#endif
#ifdef HAVE_DARWIN
        res = ptrace(PT_ATTACHEXC, pid, nullptr, 0);
#endif

        if (res == -1)
        {
            Log::error() << "Could not attach to pid " << pid
                         << ". Try setting /proc/sys/kernel/yama/ptrace_scope to 0";
            throw_errno();
        }
    }
    if (pid == -1)
    {
        Log::error() << "Fork failed.";
        throw_errno();
    }
    else if (pid == 0)
    {
        assert(spawn);
        run_command(config().command);
    }
    else
    {
        std::string proc_name;
        if (spawn)
        {
            proc_name = config().command.at(0);
        }
        else
        {
            proc_name = get_process_exe(pid);
        }

#ifdef HAVE_LINUX
        ProcessController controller(pid, proc_name, spawn, monitor);
        controller.run();
#endif
#ifdef HAVE_DARWIN
        // TODO implement the process mode
        (void)monitor;
#endif
    }
}
} // namespace monitor
} // namespace lo2s
