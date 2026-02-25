#include <lo2s/config/accelerator_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <vector>

#include <cstdint>
#include <cstdlib>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace lo2s
{
AcceleratorConfig::AcceleratorConfig(nitro::options::arguments& arguments)
{
    nec_read_interval =
        std::chrono::microseconds(arguments.as<std::uint64_t>("nec-readout-interval"));

    nec_check_interval =
        std::chrono::milliseconds(arguments.as<std::uint64_t>("nec-check-interval"));

    for (const auto& accel : arguments.get_all("accel"))
    {
        if (accel == "nec")
        {
#ifdef HAVE_VEOSINFO
            nec = true;
#else
            std::cerr << "lo2s was built without support for NEC SX-Aurora sampling\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else if (accel == "nvidia")
        {
#ifdef HAVE_CUDA
            nvidia = true;
#else
            std::cerr << "lo2s was built without support for CUDA kernel recording\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else if (accel == "openmp")
        {
#ifdef HAVE_OPENMP
            openmp = true;
#else
            std::cerr << "lo2s was built without support for OpenMP recording\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else if (accel == "amd")
        {
#ifdef HAVE_HIP
            hip = true;
#else
            std::cerr << "lo2s was built without support for AMD HIP recording\n";
            std::exit(EXIT_FAILURE);
#endif
        }
        else
        {
            std::cerr << "Unknown Accelerator " << accel << "!";
            std::exit(EXIT_FAILURE);
        }
    }
}

void AcceleratorConfig::add_parser(nitro::options::parser& parser)
{
    auto& accel_options = parser.group("Accelerator options");
    std::vector<std::string> accelerators;

#ifdef HAVE_CUDA
    accelerators.emplace_back("nvidia");
#endif
#ifdef HAVE_VEOSINFO
    accelerators.emplace_back("nec");
#endif
#ifdef HAVE_OPENMP
    accelerators.emplace_back("openmp");
#endif

    accel_options
        .multi_option(
            "accel",
            fmt::format("Accelerator to record execution events for. Available accelerators: {}",
                        fmt::join(accelerators, ", ")))
        .metavar("ACCEL")
        .optional();
    accel_options.option("nec-readout-interval", "Accelerator sampling interval")
        .optional()
        .metavar("USEC")
        .default_value("1");
    accel_options.option("nec-check-interval", "The interval between checks for new VE processes")
        .optional()
        .metavar("MSEC")
        .default_value("100");
}

void to_json(nlohmann::json& j, const AcceleratorConfig& config)
{
    j = nlohmann::json({ { "nvidia", config.nvidia },
                         { "openmp", config.openmp },
                         { "hip", config.hip },
                         { "nec", config.nec },
                         { "nec_read_interval", config.nec_read_interval.count() },
                         { "nec_check_interval", config.nec_check_interval.count() } });
}
} // namespace lo2s
