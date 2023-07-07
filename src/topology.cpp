#include <lo2s/topology.hpp>
#include <lo2s/util.hpp>

#ifdef HAVE_NVML
extern "C"
{
#include <nvml.h>
}
#endif

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

    #ifdef HAVE_NVML
    // get GPUs from nvml
    nvmlReturn_t result;
    unsigned int device_count;

    // First initialize NVML library
    result = nvmlInit();

    if (NVML_SUCCESS != result)
    { 

        Log::error() << "Failed to initialize NVML: " << nvmlErrorString(result);
    }

    // Get number of GPUs
    result = nvmlDeviceGetCount(&device_count);

    if (NVML_SUCCESS != result)
    { 

        Log::error() << "Failed to query device count: " << nvmlErrorString(result);
    }

    for(unsigned int i = 0; i < device_count; i++)
    {
        // Get GPU handle and name
        nvmlDevice_t device;
        char name[96];

        result = nvmlDeviceGetHandleByIndex(i, &device);

        if (NVML_SUCCESS != result)
        { 

            Log::error() << "Failed to get handle for device: " << nvmlErrorString(result);
        }

        result = nvmlDeviceGetName(device, name, sizeof(name) / sizeof(name[0]));

        if (NVML_SUCCESS != result)
        { 

            Log::error() << "Failed to get name for device: " << nvmlErrorString(result);
        }

        gpus_.emplace(i, name);
    }

    result = nvmlShutdown();

    if (NVML_SUCCESS != result)
    {

        Log::error() << "Failed to shutdown NVML: " << nvmlErrorString(result);
    }
    
    #endif
 
}

} // namespace lo2s
