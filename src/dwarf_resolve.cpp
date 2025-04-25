/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2025,
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

#include <lo2s/dwarf_resolve.hpp>

#include <lo2s/config.hpp>

#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>

#ifdef HAVE_DEBUGINFOD
extern "C"
{
#include <elfutils/debuginfod.h>
}
#endif

namespace lo2s
{

struct getfuncs_arg
{
    Dwarf_Addr addr;
    std::string name;
};

/*
 * libelf provides a dwarf_getfuncs() function, which calls a callback for every
 * function type Dwarf Debugging Information Entry (DIE) found in a Compilation Unit (CU)
 * DIE.
 *
 * Our implementation of that callback simply checks if the address we are looking up is found in
 * that function DIE, if yes, recording the name of the function DIE and aborting the search.
 */
static int find_containing_func_for_addr(Dwarf_Die* d, void* arg)
{
    struct getfuncs_arg* args = reinterpret_cast<struct getfuncs_arg*>(arg);

    if (dwarf_haspc(d, args->addr))
    {
        const char* name = dwarf_diename(d);
        if (name != nullptr)
        {
            args->name = name;
        }
        return DWARF_CB_ABORT;
    }

    return DWARF_CB_OK;
}

std::unique_ptr<indicators::ProgressBar> bar;

/*
 * Debuginfod reports the progress of the currently downloading debug information to this function.
 * Use this info to display a nice progress bar if stderr is a tty
 */
#ifdef HAVE_DEBUGINFOD
static int progress_fn([[maybe_unused]] debuginfod_client* c, long a, long b)
{
    if (b != 0)
    {

        if (logging::get_min_severity_level() <= nitro::log::severity_level::info)
        {
            bar->set_progress(((double)a / (double)b) * 100);
        }
    }
    return 0;
}
#endif

/*
 * Function which is called when libdwfl needs to lookup the associated debug information file for
 * an object file. In our case, this is simply a wrapper around the standard
 * dwfl_standard_find_debuginfo function, which records some additional information to be used in th
 * progress_fn() callback.
 */
static int standard_find_debuginfo_wrapper(Dwfl_Module* mod, void** userdata, const char* modname,
                                           Dwarf_Addr base, const char* file_name,
                                           const char* debuglink_file, GElf_Word debuglink_crc,
                                           char** debuginfo_file_name)
{
    if (logging::get_min_severity_level() <= nitro::log::severity_level::info &&
        isatty(STDERR_FILENO))
    {
        bar = std::make_unique<indicators::ProgressBar>(
            indicators::option::PrefixText(fmt::format("Downloading debuginfo ", file_name)));

        Log::info() << "Looking up debuginfo for: " << file_name;
        indicators::show_console_cursor(false);
    }

    int res = dwfl_standard_find_debuginfo(mod, userdata, modname, base, file_name, debuglink_file,
                                           debuglink_crc, debuginfo_file_name);

    if (logging::get_min_severity_level() <= nitro::log::severity_level::info &&
        isatty(STDERR_FILENO))
    {
        indicators::show_console_cursor(true);
    }

    return res;
}

/*
 * If DWARF support is completely disabled, use this cb for debug information lookup. This always
 * reports that no DWARF file could be found.
 */
static int dwfl_dummy_find_debuginfo(Dwfl_Module*, void**, const char*, GElf_Addr, const char*,
                                     const char*, GElf_Word, char**)
{
    return -1;
}

DwarfFunctionResolver::DwarfFunctionResolver(std::string name) : FunctionResolver(name)
{
    if (config().dwarf == DwarfUsage::NONE)
    {
        cb.find_debuginfo = dwfl_dummy_find_debuginfo;
    }
    else
    {
        cb.find_debuginfo = standard_find_debuginfo_wrapper;
    }

    cb.debuginfo_path = nullptr;
    cb.find_elf = dwfl_linux_proc_find_elf;
    cb.section_address = dwfl_offline_section_address;

    dwfl_ = dwfl_begin(&cb);

    if (dwfl_ == nullptr)
    {
        Log::error() << "Could not open dwfl session: " << dwfl_errmsg(dwfl_errno());
        throw std::runtime_error(dwfl_errmsg(dwfl_errno()));
    }

#ifdef HAVE_DEBUGINFOD
    debuginfod_client* debug_c = dwfl_get_debuginfod_client(dwfl_);

    if (logging::get_min_severity_level() <= nitro::log::severity_level::info &&
        isatty(STDERR_FILENO))
    {
        if (debug_c != 0)
        {
            debuginfod_set_progressfn(debug_c, progress_fn);
        }
    }

    if (logging::get_min_severity_level() <= nitro::log::severity_level::trace)
    {
        if (debug_c != 0)
        {
            // This displays quite a lot of additional information from debuginfod, so gate it
            // behind log level trace
            debuginfod_set_verbose_fd(debug_c, STDERR_FILENO);
        }
    }
#endif

    dwfl_report_begin(dwfl_);

    // Open the object file in the current dwfl session
    mod_ = dwfl_report_offline(dwfl_, name.c_str(), name.c_str(), -1);
    if (mod_ == nullptr)
    {
        throw std::runtime_error(dwfl_errmsg(dwfl_errno()));
    }
    dwfl_report_end(dwfl_, NULL, NULL);
}

DwarfFunctionResolver::~DwarfFunctionResolver()
{
    dwfl_end(dwfl_);
}

LineInfo DwarfFunctionResolver::lookup_line_info(Address addr)
{
    if (cache_.count(addr))
    {
        return cache_.at(addr);
    }

    // Get the name of the current module (e.g. "libfoo.so")
    const char* module_name =
        dwfl_module_info(mod_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (config().dwarf != DwarfUsage::NONE)
    {
        Dwarf_Die* cudie = nullptr;
        Dwarf_Addr bias;

        /*
         * On the top level DWARF debug information consists of DIE entries for
         * the different compilation units. Iterate over the lot of them and check
         * if they contain the instruction pointer we are searching for
         */
        while ((cudie = dwfl_module_nextcu(mod_, cudie, &bias)) != nullptr)
        {
            if (dwarf_haspc(cudie, addr.value()))
            {
                // Get line info and src file information ( e.g. "foo.c:42")
                Dwarf_Line* line = dwarf_getsrc_die(cudie, addr.value());
                int lineno;
                dwarf_lineno(line, &lineno);
                const char* srcname = dwarf_linesrc(line, nullptr, nullptr);

                struct getfuncs_arg arg;
                arg.addr = addr.value();
                arg.name = "";

                // Iterate over all function DIEs in the Compilation Unit DIE, trying to find the
                // one that contains addr.
                dwarf_getfuncs(cudie, find_containing_func_for_addr, &arg, 0);

                if (arg.name != "")
                {
                    return cache_
                        .emplace(addr, LineInfo::for_function(srcname, arg.name.c_str(), lineno,
                                                              module_name))
                        .first->second;
                }
            }
        }
    }

    // Fall back  to symbol table if we had no luck with the DWARF information
    for (int i = 0; i < dwfl_module_getsymtab(mod_); i++)
    {
        GElf_Sym sym;       // ELF Symbol data structure
        GElf_Addr sym_addr; // Starting address of the symbol
        Dwarf_Addr bias; // Needs to subtracted from the sym_addr to get the real sym_addr in memory
                         // at runtime.

        const char* name =
            dwfl_module_getsym_info(mod_, i, &sym, &sym_addr, nullptr, nullptr, &bias);

        if (addr.value() >= (sym_addr - bias) && addr.value() < (sym_addr + sym.st_size - bias))
        {
            return cache_
                .emplace(Range(sym_addr - bias, sym_addr + sym.st_size - bias),
                         LineInfo::for_function(module_name, name, 1, module_name))
                .first->second;
        }
    }
    return cache_.emplace(addr, LineInfo::for_binary(module_name)).first->second;
}
} // namespace lo2s
