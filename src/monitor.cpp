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

#include <lo2s/monitor.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/perf/tracepoint/recorder.hpp>
#include <lo2s/trace/counters.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <exception>
#include <system_error>

#include <cstdlib>
#include <cstring>

extern "C" {
#include <signal.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
}

// used for attaching to a running process.
// Its a special case, so ... OH, look behind you, a three headed monkey playing cards.
static volatile bool running;
static volatile pid_t attached_pid;

namespace lo2s
{

/* this signal handler is called once to print out stats on abort */
void sig_handler(int signum)
{
    // If we attached to a running process, we initiate the detaching now. Sick.
    if (signum == SIGINT && attached_pid != -1)
    {
        Log::info() << "Received SIGINT. Trying to detach";

        // set global running to false
        running = false;

        // send attached process SIGSTOP, so the tracee enters signal-delivery-stop
        kill(attached_pid, SIGSTOP);

        return;
    }

    Log::debug() << "sig_handler called, what should I do with signal " << signum;
    // TODO: check signum
    /* restore default signal hander */
    // signal(SIGINT, default_signal_handler);
}

void check_ptrace(enum __ptrace_request request, pid_t pid, void* addr = nullptr,
                  void* data = nullptr)
{
    auto retval = ptrace(request, pid, addr, data);
    if (retval == -1)
    {
        auto ex = make_system_error();
        Log::warn() << "Failed ptrace call: " << request << ", " << pid << ", " << addr << ", "
                    << data << ": " << ex.what();
        throw ex;
    }
}

void check_ptrace_setoptions(pid_t pid, long options)
{
    check_ptrace(PTRACE_SETOPTIONS, pid, NULL, (void*)options);
}

Monitor::Monitor(pid_t child, const std::string& name, trace::Trace& trace_, bool spawn,
                 const MonitorConfig& config)
: first_child_(child), threads_(*this), default_signal_handler(signal(SIGINT, sig_handler)),
  time_converter_(), trace_(trace_),
  counters_metric_class_(trace::Counters::get_metric_class(trace_)), config_(config),
  metrics_(trace_)
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

    // try to initialize raw counter metrics
    if (!config_.tracepoint_events.empty())
    {
        try
        {
            raw_counters_ =
                std::make_unique<perf::tracepoint::Recorder>(trace_, config_, time_converter_);
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize tracepoint events: " << e.what();
        }
    }

    // notify the trace, that we are ready to start. That means, get_time() of this call will be
    // the first possible timestamp in the trace
    trace_.begin_record();

    trace_.process(child, name);
    threads_.insert(child, child, spawn);
}

Monitor::~Monitor()
{
    // Notify trace, that we will end recording now. That means, get_time() of this call will be
    // the last possible timestamp in the trace
    trace_.end_record();

    if (raw_counters_)
    {
        raw_counters_->stop();
    }
}

void Monitor::run()
{
    // monitor fork/exit/... via ptrace
    while (1)
    {
        /* wait for signal */
        int status;
        pid_t child = waitpid(-1, &status, __WALL);
        handle_signal(child, status);
    }
}

void Monitor::handle_ptrace_event_stop(pid_t child, int event)
{
    Log::debug() << "PTRACE_EVENT-stop for child " << child << ": " << event;
    if ((event == PTRACE_EVENT_FORK) || (event == PTRACE_EVENT_VFORK))
    {
        // we need the pid of the new process
        pid_t newpid;
        check_ptrace(PTRACE_GETEVENTMSG, child, NULL, &newpid);
        auto name = get_process_exe(newpid);
        Log::debug() << "New process is forked " << newpid << ": " << name << " parent: " << child
                     << ": " << get_process_exe(child);

        trace_.process(newpid, name);
        threads_.insert(newpid, newpid, false);
    }
    // new Thread?
    else if (event == PTRACE_EVENT_CLONE)
    {
        // we need the tid of the new process
        long newpid;
        check_ptrace(PTRACE_GETEVENTMSG, child, NULL, &newpid);

        // Parent may be a thread, get the process
        auto pid = threads_.get_thread(child).pid();

        Log::info() << "New thread is cloned " << newpid << " parent: " << child << " pid: " << pid;

        // register monitoring
        threads_.insert(pid, newpid, false);
    }
    // process or thread exited?
    if (event == PTRACE_EVENT_EXIT)
    {
        auto& thread = threads_.get_thread(child);
        Log::info() << "Thread/process " << thread.tid() << " is about to exit";

        thread.disable();
        threads_.try_join();
    }
}

void Monitor::handle_signal(pid_t child, int status)
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
            check_ptrace_setoptions(child, PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
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
                handle_ptrace_event_stop(child, status >> 16);
            }
            break;
        }
        // only catch lethal signals
        case SIGHUP:  /* fall-through */
        case SIGINT:  /* fall-through */
        case SIGQUIT: /* fall-through */
        case SIGILL:  /* fall-through */
        case SIGABRT: /* fall-through */
        case SIGFPE:  /* fall-through */
        case SIGKILL: /* fall-through */
        case SIGSEGV: /* fall-through */
        case SIGPIPE: /* fall-through */
        case SIGTERM:
            // exit if first child (the original sampled process) got killed by a signal
            if (child == first_child_)
            {
                Log::error() << "Process stopped with signal" << strsignal(WSTOPSIG(status));
                throw std::system_error(std::make_error_code(std::errc::interrupted),
                                        strsignal(WSTOPSIG(status)));
            }
            break;
        default:
            Log::debug() << "Forwarding signal for child " << child << ": " << WSTOPSIG(status);
            // Stupid double cast to prevent warning -Wint-to-void-pointer-cast
            check_ptrace(PTRACE_CONT, child, nullptr, (void*)((long)WSTOPSIG(status)));
            return;
        }
    }
    else if (WIFEXITED(status))
    {
        // exit if first child (the original sampled process) is dead
        if (child == first_child_)
        {
            // NOTE: Yes we even throw the error success here!
            Log::info() << "Exiting monitor with status " << WEXITSTATUS(status);
            throw std::system_error(WEXITSTATUS(status), std::system_category());
        }
        return;
    }
    else
    {
        Log::warn() << "Unknown signal for child " << child << ": " << status;
    }
    // wait for next fork/exit/...
    check_ptrace(PTRACE_CONT, child);
}
}
