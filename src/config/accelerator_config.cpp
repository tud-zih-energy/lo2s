// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    for (const auto& accel : arguments.get_all("accel"))
    {
        if (accel == "nvidia")
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
}

void to_json(nlohmann::json& j, const AcceleratorConfig& config)
{
    j = nlohmann::json({
        { "nvidia", config.nvidia },
        { "openmp", config.openmp },
        { "hip", config.hip },
    });
}
} // namespace lo2s
