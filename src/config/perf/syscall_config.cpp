#include <lo2s/config/perf/syscall_config.hpp>

#include <lo2s/log.hpp>
#include <lo2s/syscalls.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <cstdint>
#include <cstdlib>

namespace lo2s::perf
{

namespace
{

std::vector<int64_t> parse_syscall_names(const std::vector<std::string>& requested_syscalls)
{
    std::vector<int64_t> syscall_nrs;
    for (const auto& requested_syscall : requested_syscalls)
    {
        const int64_t syscall_nr = syscall_nr_for_name(requested_syscall);
        if (syscall_nr == -1)
        {
            Log::warn() << "syscall: " << requested_syscall << " not recognized!";
            std::exit(EXIT_FAILURE);
        }
        syscall_nrs.emplace_back(syscall_nr);
    }
    return syscall_nrs;
}

} // namespace

SyscallConfig::SyscallConfig(nitro::options::arguments& arguments)
{
    if (arguments.provided("syscall"))
    {
        std::vector<std::string> requested_syscalls = arguments.get_all("syscall");
        enabled = true;

        if (std::find(requested_syscalls.begin(), requested_syscalls.end(), "all") ==
            requested_syscalls.end())
        {
#ifdef HAVE_LIBAUDIT
            syscalls = parse_syscall_names(requested_syscalls);
#else
            Log::warn() << "lo2s was compiled without libaudit support and can thus not resolve "
                           "syscall names, try the syscall number instead";
#endif
        }
    }
}

void SyscallConfig::add_parser(nitro::options::parser& parser)
{
    auto& syscall_options = parser.group("Syscall recording options");
    syscall_options
        .multi_option("syscall",
                      "Record syscall events for given syscall. \"all\" to record all syscalls")
        .short_name("s")
        .metavar("SYSCALL")
        .optional();
}

void to_json(nlohmann::json& j, const SyscallConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled }, { "syscalls", config.syscalls } });
}
} // namespace lo2s::perf
