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

#include <lo2s/process_controller.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <exception>
#include <limits>
#include <system_error>

#include <csignal>
#include <cstdlib>
#include <cstring>

extern "C"
{
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
}

// used for attaching to a running process.
// Its a special case, so ... OH, look behind you, a three headed monkey playing cards.
static volatile std::sig_atomic_t running;
static volatile std::sig_atomic_t attached_pid;
static_assert(std::numeric_limits<pid_t>::max() <= std::numeric_limits<std::sig_atomic_t>::max(),
              ":[");

extern "C" void sig_handler(int signum)
{
    // If we attached to a running process, we initiate the detaching now. Sick.
    if (signum == SIGINT && attached_pid != -1)
    {
        // set global running to false
        running = false;

        // send attached process SIGSTOP, so the tracee enters signal-delivery-stop
        kill(attached_pid, SIGSTOP);

        return;
    }

    // Log is certainly not signal safe
    // lo2s::Log::debug() << "sig_handler called, what should I do with signal " << signum;

    // For some miraculous reason, the signal ends up at the monitored process anyhow, even if we
    // do not explicitly forward it here.

    /* restore default signal handler */
    // signal(SIGINT, default_signal_handler);
}

namespace lo2s
{

static void ptrace_checked_call(enum __ptrace_request request, pid_t pid, void* addr = nullptr,
                                void* data = nullptr)
{
    long retval = ptrace(request, pid, addr, data);
    if (retval == -1)
    {
        auto ex = make_system_error();
        Log::info() << "Failed ptrace call: " << request << ", " << pid << ", " << addr << ", "
                    << data << ": " << ex.what();
        throw ex;
    }
}

static void ptrace_setoptions(pid_t pid, long options)
{
    ptrace_checked_call(PTRACE_SETOPTIONS, pid, nullptr, (void*)options);
}

static unsigned long ptrace_geteventmsg(pid_t pid)
{
    unsigned long msgval;
    ptrace_checked_call(PTRACE_GETEVENTMSG, pid, nullptr, &msgval);
    return msgval;
}

static void ptrace_cont(pid_t pid, long signum = 0)
{
    // ptrace(2) mandates passing the signal number in the data parameter.
    ptrace_checked_call(PTRACE_CONT, pid, nullptr, reinterpret_cast<void*>(signum));
}

ProcessController::ProcessController(pid_t child, const std::vector<std::string>& command_line,
                                     bool spawn, monitor::AbstractProcessMonitor& monitor)
: first_child_(child), default_signal_handler(signal(SIGINT, sig_handler)), monitor_(monitor),
  num_wakeups_(0)
{
    if (spawn)
    {
        attached_pid = -1;
    }
    else
    {
        attached_pid = child;
    }
    running = true;

    watched_threads_.add_main_thread_for_process(child);

    monitor_.insert_process(child, trace::Trace::NO_PARENT_PROCESS_PID, command_line, spawn);

    summary().add_thread();
}

ProcessController::~ProcessController()
{
    summary().record_perf_wakeups(num_wakeups_);
}
void ProcessController::run()
{
    // monitor fork/exit/... via ptrace
    while (1)
    {
        /* wait for signal */
        int status;
        pid_t child = waitpid(-1, &status, __WALL);

        num_wakeups_++;

        handle_signal(child, status);
    }
}

void ProcessController::handle_ptrace_event(pid_t child, int event)
{
    Log::debug() << "PTRACE_EVENT-stop for child " << child << ": " << event;
    switch (event)
    {
    case PTRACE_EVENT_FORK:
    case PTRACE_EVENT_VFORK:
    {
        // (v)fork was called in child, register the forked process.
        try
        {
            // we need the pid of the new process
            pid_t new_pid = ptrace_geteventmsg(child);

            std::string command = get_process_comm(new_pid);
            std::vector<std::string> cmdline = get_process_cmdline(new_pid);
            Log::debug() << "New process " << new_pid << " (" << command << "): forked from "
                         << child;

            // Register the newly created process for monitoring:
            // (1) Associate a main thread with this process.
            watched_threads_.add_main_thread_for_process(new_pid);
            // (2) Tell the process monitor to watch a new process.
            monitor_.insert_process(new_pid, child, cmdline);
            // (3) Update our summary information.
            summary().add_thread();
        }
        catch (std::system_error& e)
        {
            Log::error() << "Failure while adding new process forked from " << child << ": "
                         << e.what();
            throw;
        }
    }
    break;
    case PTRACE_EVENT_CLONE:
    {
        // Child was cloned, figure out the process in which the clone happened
        // and register the newly created thread.

        try
        {
            // we need the tid of the new process
            pid_t new_tid = ptrace_geteventmsg(child);

            // Thread may have been clone from another thread, figure out which
            // process they both belong too.
            pid_t pid = watched_threads_.get_process_for_thread(child);
            std::string command = get_task_comm(pid, new_tid);
            Log::info() << "New thread " << new_tid << " (" << command << "): cloned from " << child
                        << " in process " << pid;

            // Register the newly created thread for monitoring:
            // (1) Keep track of the process the new thread was spawned in.
            watched_threads_.add_thread_to_process(new_tid, pid);
            // (2) Tell the process monitor to watch the new thread.
            monitor_.insert_thread(pid, new_tid, command);
            // (3) Update our summary information.
            summary().add_thread();
        }
        catch (std::out_of_range& e)
        {
            Log::error() << "Failed to get process containing monitored thread " << child;
        }
        catch (std::system_error& e)
        {
            Log::error() << "Failure while adding new thread cloned from " << child << ": "
                         << e.what();
            throw;
        }
    }
    break;
    case PTRACE_EVENT_EXIT:
    {
        // Child is about to exit. Signal its monitor to stop, then clean up.

        try
        {
            pid_t pid = watched_threads_.get_process_for_thread(child);
            if (pid == child)
            {
                int exit_status = ptrace_geteventmsg(pid);
                Log::info() << "Process " << child << " is about to exit with status "
                            << exit_status;
                monitor_.exit_process(child, exit_status);
            }
            else
            {
                Log::info() << "Thread  " << child << " in process " << pid << " is about to exit";
                monitor_.exit_thread(child);
            }
            watched_threads_.remove_thread(child);
        }
        catch (std::out_of_range&)
        {
            Log::warn() << "Thread " << child << " is about to exit, but was never seen before.";
        }
    }
    break;
    default:
        Log::warn() << "Unhandled ptrace event for child " << child << ": " << event;
    }
}

void ProcessController::handle_signal(pid_t child, int status)
{
    Log::debug() << "Handling signal " << status << " from child: " << child;
    if (WIFSTOPPED(status)) // signal-delivery-stop
    {
        Log::debug() << "signal-delivery-stop from child " << child << ": " << WSTOPSIG(status);

        // Special handling for detaching, then we had just attached to a process
        if (attached_pid != -1 && !running)
        {
            Log::debug() << "Detaching from child: " << child;

            // Tracee is in signal-delivery-stop, so we can detach
            ptrace(PTRACE_DETACH, child, 0, status);

            // exit if detached from first child (the original sampled process)
            if (child == first_child_)
            {
                Log::info() << "Exiting monitor with status " << 0;
                throw std::system_error(0, std::system_category());
            }
        }

        switch (WSTOPSIG(status))
        {
        case SIGSTOP:
        {
            Log::debug() << "Set ptrace options for process: " << child;

            // we are only interested in fork/join events
            ptrace_setoptions(child, PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
                                         PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT);
            // FIXME TODO continue this new thread/process ONLY if already registered in the
            // thread map.
            break;
        }
        case SIGTRAP: // maybe a different kind of ptrace-stop
        {
            auto event = status >> 16;
            if (event != 0)
            {
                handle_ptrace_event(child, event);
            }
            break;
        }

        default:
            Log::debug() << "Forwarding signal for child " << child << ": " << WSTOPSIG(status);
            ptrace_cont(child, WSTOPSIG(status));
            return;
        }
    }
    else if (WIFEXITED(status))
    {
        Log::info() << "Process " << child << " exiting with status " << WEXITSTATUS(status);

        // exit if first child (the original sampled process) is dead
        if (child == first_child_)
        {
            summary().set_exit_code(WEXITSTATUS(status));
            throw std::system_error(0, std::system_category());
        }
        return;
    }
    else if (WIFSIGNALED(status))
    {
        Log::info() << "Process " << child << " exited due to signal " << WTERMSIG(status);
        // exit if first child (the original sampled process) is dead
        if (child == first_child_)
        {
            throw std::system_error(0, std::system_category());
        }
        return;
    }
    else
    {
        Log::warn() << "Unknown signal for child " << child << ": " << status;
    }
    // wait for next fork/exit/...
    try
    {
        ptrace_cont(child);
    }
    catch (const std::system_error& e)
    {
        if (e.code().value() == ESRCH)
        {
            Log::info() << "Received ESRCH for PTRACE_CONT on " << child;
        }
        else
        {
            throw e;
        }
    }
}
} // namespace lo2s
