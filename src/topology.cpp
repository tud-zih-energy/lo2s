#include <lo2s/topology.hpp>

#include <lo2s/types/cpu.hpp>
#include <lo2s/types/package.hpp>
#include <lo2s/util.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include <cstdint>

#include <fmt/format.h>

namespace lo2s
{

void Topology::read_proc()
{
    const std::filesystem::path base_path = "/sys/devices/system/cpu";
    auto online = parse_list_from_file(base_path / "online");

    for (auto cpu_id : online)
    {
        std::filesystem::path const topology =
            base_path / fmt::format("cpu{}", cpu_id) / "topology";
        std::ifstream package_stream(topology / "physical_package_id");
        std::ifstream core_stream(topology / "core_id");

        uint32_t package_id = 0;
        uint32_t core_id = 0;
        package_stream >> package_id;
        core_stream >> core_id;

        cpus_.emplace(cpu_id);
        packages_.emplace(package_id);
        cpu_to_core_.emplace(Cpu(cpu_id), Core(core_id, package_id));
        cpu_to_package_.emplace(Cpu(cpu_id), Package(package_id));
    }

    std::string line;
    std::ifstream cpuinfo("/proc/cpuinfo");

    while (std::getline(cpuinfo, line))
    {
        // if only C++14 had starts_with :(
        if (line.find("flags") == 0)
        {
            if (line.find("hypervisor") != std::string::npos)
            {
                hypervised_ = true;
                break;
            }
        }
    }
}

} // namespace lo2s
