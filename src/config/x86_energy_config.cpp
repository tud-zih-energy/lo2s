#include <lo2s/config/x86_energy_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

namespace lo2s
{
X86EnergyConfig::X86EnergyConfig(nitro::options::arguments& arguments)
{
    enabled = arguments.given("x86-energy");
}

void X86EnergyConfig::add_parser(nitro::options::parser& parser)
{
    auto& x86_energy_options = parser.group("x86_energy options");
    x86_energy_options.toggle("x86-energy", "Add x86_energy recordings.").short_name("X");
}

void X86EnergyConfig::check() const
{
#ifndef HAVE_X86_ENERGY
    if (enabled)
    {
        lo2s::Log::fatal() << "lo2s was built without support for x86_energy; "
                              "cannot request x86_energy readings.\n";
        std::exit(EXIT_FAILURE);
    }
#endif
}

void to_json(nlohmann::json& j, const X86EnergyConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled } });
}

} // namespace lo2s
