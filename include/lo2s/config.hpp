// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/config/accelerator_config.hpp>
#include <lo2s/config/dwarf_config.hpp>
#include <lo2s/config/general_config.hpp>
#include <lo2s/config/otf2_config.hpp>
#include <lo2s/config/perf_config.hpp>
#include <lo2s/config/put_config.hpp>
#include <lo2s/config/rb_config.hpp>
#include <lo2s/config/sensors_config.hpp>
#include <lo2s/config/x86_adapt_config.hpp>
#include <lo2s/config/x86_energy_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json_fwd.hpp>

namespace lo2s
{

struct Config
{
    Config(nitro::options::arguments& arguments, int argc, const char** argv)
    : general(arguments, argc, argv), otf2(arguments), put(arguments), perf(arguments),
      dwarf(arguments), x86_energy(arguments), x86_adapt(arguments), sensors(arguments),
      accel(arguments), rb(arguments)

    {
    }

    GeneralConfig general;
    Otf2Config otf2;
    ProgramUnderTestConfig put;
    perf::Config perf;

    DwarfConfig dwarf;

    X86EnergyConfig x86_energy;

    X86AdaptConfig x86_adapt;

    SensorsConfig sensors;

    AcceleratorConfig accel;
    RingbufConfig rb;

    static void add_parser(nitro::options::parser& parser);

    void check() const;

    static void parse_print_options(nitro::options::arguments& arguments)
    {
        perf::Config::parse_print_options(arguments);
        X86AdaptConfig::parse_print_options(arguments);
    }

    static void set_verbosity(nitro::options::arguments& arguments)
    {
        GeneralConfig::set_verbosity(arguments);
    }
};

const Config& config();

void parse_program_options(int argc, const char** argv);

void to_json(nlohmann::json& j, const Config& config);
} // namespace lo2s
