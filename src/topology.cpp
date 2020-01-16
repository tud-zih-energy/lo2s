#include <lo2s/topology.hpp>

namespace lo2s
{
const fs::path Topology::base_path = "/sys/devices/system/cpu";

namespace detail
{
static auto parse_list(std::string list) -> std::set<uint32_t>
{
    std::stringstream s;
    s << list;

    std::set<uint32_t> res;

    std::string part;
    while (std::getline(s, part, ','))
    {
        auto pos = part.find('-');
        if (pos != std::string::npos)
        {
            // is a range
            uint32_t from = std::stoi(part.substr(0, pos));
            uint32_t to = std::stoi(part.substr(pos + 1));

            for (auto i = from; i <= to; ++i)
                res.insert(i);
        }
        else
        {
            // single value
            res.insert(std::stoi(part));
        }
    }

    return res;
}
} // namespace detail

void Topology::read_proc()
{
    std::string online_list;
    std::string present_list;

    {
        fs::ifstream cpu_online(base_path / "/online");
        std::getline(cpu_online, online_list);

        fs::ifstream cpu_present(base_path / "/present");
        std::getline(cpu_present, present_list);
    }

    auto online = detail::parse_list(online_list);
    auto present = detail::parse_list(present_list);

    for (auto cpu_id : online)
    {
        std::stringstream filename_stream;

        fs::path topology = base_path / ("cpu"s + std::to_string(cpu_id)) / "topology";
        fs::ifstream package_stream(topology / "physical_package_id");
        fs::ifstream core_stream(topology / "core_id");

        uint32_t package_id, core_id;
        package_stream >> package_id;
        core_stream >> core_id;

        auto insert_package = packages_.insert(std::make_pair(package_id, Package(package_id)));
        auto& package = insert_package.first->second;
        if (package.id != package_id)
        {
            throw std::runtime_error("Inconsistent package ids in topology.");
        }
        package.core_ids.insert(core_id);
        package.cpu_ids.insert(cpu_id);

        auto insert_core = cores_.insert(
            std::make_pair(std::make_tuple(package_id, core_id), Core(core_id, package_id)));
        auto& core = insert_core.first->second;
        if (core.id != core_id || core.package_id != package_id)
        {
            throw std::runtime_error("Inconsistent package/core ids in topology.");
        }
        core.cpu_ids.insert(cpu_id);

        auto insert_cpu = cpus_.insert(std::make_pair(cpu_id, Cpu(cpu_id, core_id, package_id)));
        auto& cpu = insert_cpu.first->second;
        (void)cpu;
        if (cpu.id != cpu_id || cpu.core_id != core_id || cpu.package_id != package_id)
        {
            throw std::runtime_error("Inconsistent cpu/package/core ids in topology.");
        }
        if (!insert_cpu.second)
        {
            throw std::runtime_error("Duplicate cpu_id when reading the topology.");
        }
    }

    {
        std::string line;
        fs::ifstream cpuinfo("/proc/cpuinfo");

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
}

} // namespace lo2s
