/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

#include <lo2s/dtrace/event_reader.hpp>

#include <lo2s/log.hpp>

#include <cassert>

extern "C"
{
#include <dtrace.h>
}

namespace lo2s
{
namespace dtrace
{

EventReader::EventReader()
{
    int err;

    handle_ = dtrace_open(DTRACE_VERSION, 0, &err);

    if (handle_ == nullptr)
    {
        throw std::runtime_error(dtrace_errmsg(nullptr, err));
    }

    // TODO make bufsize configurable
    // TODO handle errors
    (void)dtrace_setopt(handle_, "bufsize", "4m");
    (void)dtrace_setopt(handle_, "aggsize", "4m");

    // setup buffer_callback
    (void)dtrace_handle_buffered(handle_, EventReader::buffer_callback, this);

    prog_info_ = std::make_unique<dtrace_proginfo_t>();

    Log::debug() << "dtrace initialized";
}

EventReader::~EventReader()
{
    dtrace_close(handle_);
}

void EventReader::init_dtrace_program(const std::string& script)
{
    Log::debug() << "Compiling Dtrace program: " << script;

    dtrace_prog_t* prog;
    prog = dtrace_program_strcompile(handle_, script.c_str(), DTRACE_PROBESPEC_NAME, DTRACE_C_ZDEFS,
                                     0, NULL);

    if (prog == nullptr)
    {
        Log::error() << "Failed to compile Dtrace script for sched probes.";
    }

    Log::debug() << "Executing Dtrace program";

    dtrace_program_exec(handle_, prog, prog_info_.get());
    dtrace_go(handle_);
}

void EventReader::read()
{
    int err = dtrace_work(handle_, nullptr, nullptr, EventReader::consume_callback, this);

    switch (err)
    {
    case DTRACE_WORKSTATUS_DONE:
        // TODO stop loop
        break;
    case DTRACE_WORKSTATUS_OKAY:
        break;
    default:
        throw std::runtime_error(dtrace_errmsg(handle_, err));
    }
}

int EventReader::consume_callback(const dtrace_probedata_t*, const dtrace_recdesc_t* rec, void*)
{
    // A nullptr rec indicates that we've processed the last record.
    if (rec == nullptr)
    {
        return (DTRACE_CONSUME_NEXT);
    }

    return (DTRACE_CONSUME_THIS);
}

int EventReader::buffer_callback(const dtrace_bufdata_t* bufdata, void* arg)
{
    assert(arg != nullptr);
    EventReader* rdr = static_cast<EventReader*>(arg);

    dtrace_probedesc_t* probedesc = bufdata->dtbda_probe->dtpda_pdesc;

    ProbeDesc pd{ probedesc->dtpd_provider, probedesc->dtpd_mod, probedesc->dtpd_func,
                  probedesc->dtpd_name };

    rdr->handle(bufdata->dtbda_probe->dtpda_cpu, pd, bufdata->dtbda_buffered,
                bufdata->dtbda_probe->dtpda_data);

    return (DTRACE_HANDLE_OK);
}

} // namespace dtrace
} // namespace lo2s
