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

static void ptrace_checked_call(enum __ptrace_request request, Thread thread, void* addr = nullptr,
                                void* data = nullptr)
{
    long retval = ptrace(request, thread.as_pid_t(), addr, data);
    if (retval == -1)
    {
        auto ex = make_system_error();
        Log::info() << "Failed ptrace call: " << request << ", " << thread << ", " << addr << ", "
                    << data << ": " << ex.what();
        throw ex;
    }
}

static void ptrace_setoptions(Thread thread, long options)
{
    ptrace_checked_call(PTRACE_SETOPTIONS, thread, nullptr, (void*)options);
}

static unsigned long ptrace_geteventmsg(Thread thread)
{
    unsigned long msgval;
    ptrace_checked_call(PTRACE_GETEVENTMSG, thread, nullptr, &msgval);
    return msgval;
}

static void ptrace_cont(Thread thread, long signum = 0)
{
    // ptrace(2) mandates passing the signal number in the data parameter.
    ptrace_checked_call(PTRACE_CONT, thread, nullptr, reinterpret_cast<void*>(signum));
}

ProcessController::ProcessController(Process child, const std::string& name, bool spawn,
                                     monitor::AbstractProcessMonitor& monitor)
: first_child_(child.as_thread()), default_signal_handler(signal(SIGINT, sig_handler)),
  monitor_(monitor), num_wakeups_(0), groups_(ExecutionScopeGroup::instance())
{
    if (spawn)
    {
        attached_pid = -1;
    }
    else
    {
        attached_pid = child.as_pid_t();
    }
    running = true;

    groups_.add_process(child);

    monitor_.insert_process(trace::Trace::NO_PARENT_PROCESS, child, name, spawn);

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

        handle_signal(Thread(child), status);
    }
}

void ProcessController::handle_ptrace_event(Thread child, int event)
{
    Log::debug() << "PTRACE_EVENT-stop for " << child << ": " << event;
    switch (event)
    {
    case PTRACE_EVENT_FORK:
    case PTRACE_EVENT_VFORK:
    {
        // (v)fork was called in child, register the forked process.
        try
        {
            // we need the pid of the new process
            auto new_process = Process(ptrace_geteventmsg(child));
            std::string command = get_process_comm(new_process);
            Log::debug() << "New " << new_process << " (" << command << "): forked from " << child;

            // Register the newly created process for monitoring:
            // (1) Associate a main thread with this process.
            groups_.add_process(new_process);
            // (2) ptrace might report a thread as the parent, find out the real parent process
            auto parent = groups_.get_process(child);
            // (3) Tell the process monitor to watch a new process.
            monitor_.insert_process(parent, new_process, command);
            // (4) Update our summary information.
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
            auto new_thread = Thread(ptrace_geteventmsg(child));

            // Thread may have been clone from another thread, figure out which
            // process they both belong too.
            auto process = groups_.get_process(child);
            std::string command = "<unknown>";

            try
            {
                command = get_task_comm(process, new_thread);
            }
            catch (std::system_error& e)
            {
                // very short living threads may be already gone at this point.
                // It will be fine.
            }

            Log::info() << "New " << new_thread << " (" << command << "): cloned from " << child
                        << " in " << process;

            // Register the newly created thread for monitoring:
            // (1) Keep track of the process the new thread was spawned in.
            groups_.add_thread(new_thread, process);
            // (2) Tell the process monitor to watch the new thread.
            monitor_.insert_thread(process, new_thread, command);
            // (3) Update our summary information.
            summary().add_thread();
        }
        catch (std::out_of_range& e)
        {
            Log::error() << "Failed to get process containing monitored " << child;
        }
        catch (std::system_error& e)
        {
            Log::error() << "Failure while adding new thread cloned from " << child << ": "
                         << e.what();
            throw;
        }
    }
    break;
    case PTRACE_EVENT_EXEC:
    {
        // Only in the case of EVENT_EXEC do we know that the signal come from the main thread =
        // (process)
        std::string name = get_process_comm(child.as_process());
        Log::debug() << "Exec in " << child << " (" << name << ")";
        monitor_.update_process_name(child.as_process(), name);
    }
    break;
    case PTRACE_EVENT_EXIT:
    {
        // Child is about to exit. Signal its monitor to stop, then clean up.
        try
        {
            Log::info() << "Thread  " << child << " is about to exit";
            monitor_.exit_thread(child);
        }
        catch (std::out_of_range&)
        {
            Log::warn() << child << " is about to exit, but was never seen before.";
        }
    }
    break;
    default:
        Log::warn() << "Unhandled ptrace event for " << child << ": " << event;
    }
}

void ProcessController::handle_signal(Thread child, int status)
{
    Log::debug() << "Handling signal " << status << " from " << child;
    if (WIFSTOPPED(status)) // signal-delivery-stop
    {
        Log::debug() << "signal-delivery-stop from " << child << ": " << WSTOPSIG(status);

        // Special handling for detaching, then we had just attached to a process
        if (!attached_pid && !running)
        {
            Log::debug() << "Detaching from " << child;

            // Tracee is in signal-delivery-stop, so we can detach
            ptrace(PTRACE_DETACH, child.as_pid_t(), 0, status);

            // exit if detached from first child (the original sampled process)
            if (child == first_child_)
            {
                std::cout << "[ lo2s: Child exited. Stopping measurements and closing trace. ]"
                          << std::endl;
                throw std::system_error(0, std::system_category());
            }
        }

        switch (WSTOPSIG(status))
        {
        case SIGSTOP:
        {
            Log::debug() << "Set ptrace options for " << child;

            // we are only interested in fork/join events
            ptrace_setoptions(child, PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
                                         PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT |
                                         PTRACE_O_TRACEEXEC);
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
            Log::debug() << "Forwarding signal for " << child << ": " << WSTOPSIG(status);
            ptrace_cont(child, WSTOPSIG(status));
            return;
        }
    }
    else if (WIFEXITED(status))
    {
        Log::info() << child << " exiting with status " << WEXITSTATUS(status);

        // exit if first child (the original sampled process) is dead
        if (child == first_child_)
        {
            std::cout << "[ lo2s: Child exited. Stopping measurements and closing trace. ]"
                      << std::endl;
            summary().set_exit_code(WEXITSTATUS(status));
            throw std::system_error(0, std::system_category());
        }
        return;
    }
    else if (WIFSIGNALED(status))
    {
        Log::info() << child << " exited due to signal " << WTERMSIG(status);
        // exit if first child (the original sampled process) is dead
        if (child == first_child_)
        {
            std::cout << "[ lo2s: Child exited. Stopping measurements and closing trace. ]"
                      << std::endl;
            throw std::system_error(0, std::system_category());
        }
        return;
    }
    else
    {
        Log::warn() << "Unknown signal for " << child << ": " << status;
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
