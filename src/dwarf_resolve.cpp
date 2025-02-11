#include <lo2s/config.hpp>
#include <lo2s/dwarf_resolve.hpp>

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

static int func_cb(Dwarf_Die* d, void* arg)
{
    struct getfuncs_arg* args = (struct getfuncs_arg*)arg;
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

static int dwfl_dummy_find_debuginfo(Dwfl_Module*, void**, const char*, GElf_Addr, const char*,
                                     const char*, GElf_Word, char**)
{
    return -1;
}

DwarfFunctionResolver::DwarfFunctionResolver(std::string name) : FunctionResolver(name)
{
    if (config().dwarf == DwarfUsage::FULL)
    {
        cb.find_debuginfo = dwfl_standard_find_debuginfo;
    }
    else if (config().dwarf == DwarfUsage::LOCAL)
    {
        cb.find_debuginfo = dwfl_build_id_find_debuginfo;
    }
    else
    {
        cb.find_debuginfo = dwfl_dummy_find_debuginfo;
    }
    cb.find_debuginfo = dwfl_build_id_find_debuginfo;
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
    if (debug_c != 0)
    {
        debuginfod_set_verbose_fd(debug_c, STDERR_FILENO);
    }
#endif

    dwfl_report_begin(dwfl_);

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
    const char* module_name =
        dwfl_module_info(mod_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (config().dwarf != DwarfUsage::NONE)
    {
        Dwarf_Die* cudie = nullptr;
        Dwarf_Addr bias;
        while ((cudie = dwfl_module_nextcu(mod_, cudie, &bias)) != nullptr)
        {
            if (dwarf_haspc(cudie, addr.value()))
            {

                Dwarf_Line* line = dwarf_getsrc_die(cudie, addr.value());
                int lineno;
                dwarf_lineno(line, &lineno);
                const char* srcname = dwarf_linesrc(line, nullptr, nullptr);

                struct getfuncs_arg arg;
                arg.addr = addr.value();
                arg.name = "";
                dwarf_getfuncs(cudie, func_cb, &arg, 0);

                return cache_
                    .emplace(addr,
                             LineInfo::for_function(srcname, arg.name.c_str(), lineno, module_name))
                    .first->second;
            }
        }
    }

    GElf_Addr offset;
    GElf_Sym sym;
    const char* name =
        dwfl_module_addrinfo(mod_, addr.value(), &offset, &sym, nullptr, nullptr, nullptr);

    if (name != nullptr)
    {
        return cache_
            .emplace(Range(sym.st_value, sym.st_value + sym.st_size),
                     LineInfo::for_function(module_name, name, 1, module_name))
            .first->second;
    }
    return cache_.emplace(addr, LineInfo::for_binary(module_name)).first->second;
}
} // namespace lo2s
