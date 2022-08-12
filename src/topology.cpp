#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

namespace lo2s
{
const std::filesystem::path Topology::base_path = "/sys/devices/system/cpu";

void Topology::read_proc()
{
    auto online = parse_list_from_file(base_path / "online");

    for (auto cpu_id : online)
    {
        std::stringstream filename_stream;

        std::filesystem::path topology = base_path / ("cpu"s + std::to_string(cpu_id)) / "topology";
        std::ifstream package_stream(topology / "physical_package_id");
        std::ifstream core_stream(topology / "core_id");

        uint32_t package_id, core_id;
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
