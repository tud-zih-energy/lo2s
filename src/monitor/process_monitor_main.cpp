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

#include <nitro/lang/string.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <cassert>

extern "C"
{
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <sys/prctl.h>
#include <sys/ptrace.h>
}

namespace lo2s
{
namespace monitor
{

static void drop_privileges()
{
    // get original uid & gid
    gid_t original_uid;
    uid_t original_gid;

    if (config().user != "")
    {
        auto orig_user = getpwnam(config().user.c_str());
        if (orig_user == nullptr)
        {
            Log::error() << "No user found with username '" + config().user + ".";
            throw_errno();
        }

        original_uid = orig_user->pw_uid;
        original_gid = orig_user->pw_gid;
    }
    else
    {
        // sudo is garanteed in this case, see check in config.hpp
        try
        {
            original_uid = std::stoi(std::getenv("SUDO_UID"));
            original_gid = std::stoi(std::getenv("SUDO_GID"));
        }
        catch (std::invalid_argument const& e)
        {
            Log::error() << "Cannot parse SUDO_UID/SUDO_GID into int.";
            throw_errno();
        }
        catch (std::out_of_range const& e)
        {
            Log::error() << "SUDO_UID/SUDO_GID out of range.";
            throw_errno();
        }
    }

    // drop ancillary group if root is to be dropped, this needs root permissions
    if (getuid() == 0 || getgid() == 0)
    {
        if (setgroups(1, &original_gid) != 0)
        {
            Log::error() << "Dropping ancillary group failed.";
            throw_errno();
        }
    }

    if (setgid(original_gid) != 0)
    {
        Log::error() << "Setting GID failed.";
        throw_errno();
    }

    if (setuid(original_uid) != 0)
    {
        Log::error() << "Setting UID failed.";
        throw_errno();
    }

    Log::debug() << "Dropped privileges successfully";

    assert(getuid() != 0);
    assert(getgid() != 0);
}

[[noreturn]] static void run_command(const std::vector<std::string>& command_and_args)
{
    struct rlimit initial_rlimit = initial_rlimit_fd();

    if (initial_rlimit.rlim_cur == 0)
    {
        Log::warn() << "Could not reset file descriptor limit to initial limit, the program under "
                       "measurement might behave erratically!";
    }
    else
    {
        setrlimit(RLIMIT_NOFILE, &initial_rlimit);
    }

    /* kill yourself if the parent dies */
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    /* we need ptrace to get fork/clone/... */
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);

    std::vector<char*> tmp;
    std::transform(command_and_args.begin(), command_and_args.end(), std::back_inserter(tmp),
                   [](const std::string& s) {
                       char* pc = new char[s.size() + 1];
                       std::strcpy(pc, s.c_str());
                       return pc;
                   });
    tmp.push_back(nullptr);

    Log::debug() << "Execute the command: " << nitro::lang::join(command_and_args);

    // Stop yourself so the parent tracer can do initialize the options
    raise(SIGSTOP);

    // change user if option was set
    if (config().drop_root == true)
    {
        drop_privileges();
    }

    // run the application which should be sampled
    execvp(tmp[0], &tmp[0]);

    // should not be executed -> exec failed, let's clean up anyway.
    for (auto cp : tmp)
    {
        delete[] cp;
    }
    Log::error() << "Could not execute the command: " << nitro::lang::join(command_and_args);
    throw_errno();
}

void process_monitor_main(AbstractProcessMonitor& monitor)
{

    auto process = config().process;
    assert(process.as_pid_t() != 0);
    bool spawn = (config().process == Process::invalid());

    if (spawn)
    {
        process = Process(fork());
    }
    else
    {
        // TODO Attach to all threads in a process
        if (ptrace(PTRACE_ATTACH, process.as_pid_t(), NULL, NULL) == -1)
        {
            Log::error() << "Could not attach to " << process
                         << ". Try setting /proc/sys/kernel/yama/ptrace_scope to 0";
            throw_errno();
        }
    }

    if (process == Process::invalid())
    {
        Log::error() << "Fork failed.";
        throw_errno();
    }
    else if (process.as_pid_t() == 0)
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
            proc_name = get_process_comm(process);
        }

        ProcessController controller(process, proc_name, spawn, monitor);
        controller.run();
    }
}
} // namespace monitor
} // namespace lo2s
