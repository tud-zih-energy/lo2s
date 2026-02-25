#include <lo2s/config/x86_adapt_config.hpp>

#include <lo2s/io.hpp>
#include <lo2s/metric/x86_adapt/knobs.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <map>

#include <cstdlib>

namespace lo2s
{
X86AdaptConfig::X86AdaptConfig(nitro::options::arguments& arguments)
{
    knobs = arguments.get_all("x86-adapt-knob");
    enabled = !knobs.empty();
}

void X86AdaptConfig::parse_print_options(nitro::options::arguments& arguments)
{
    if (arguments.given("list-knobs"))
    {
#ifdef HAVE_X86_ADAPT

        std::map<std::string, std::string> node_knobs = metric::x86_adapt::x86_adapt_node_knobs();
        std::cout << io::make_argument_list("x86_adapt node knobs", node_knobs.begin(),
                                            node_knobs.end());

        std::map<std::string, std::string> cpu_knobs = metric::x86_adapt::x86_adapt_cpu_knobs();
        std::cout << io::make_argument_list("x86_adapt CPU knobs", cpu_knobs.begin(),
                                            cpu_knobs.end());
        std::exit(EXIT_SUCCESS);
#else
        std::cerr << "lo2s was built without support for x86_adapt; cannot read knobs.\n";
        std::exit(EXIT_FAILURE);
#endif
    }
}

void X86AdaptConfig::add_parser(nitro::options::parser& parser)
{
    auto& x86_adapt_options = parser.group("x86_adapt options");
    x86_adapt_options
        .multi_option("x86-adapt-knob",
                      "Record the given x86_adapt knob. Append #accumulated_last for semantics.")
        .short_name("x")
        .optional()
        .metavar("KNOB");

    x86_adapt_options.toggle("list-knobs", "List available x86_adapt CPU knobs.");
}

void X86AdaptConfig::check() const
{
#ifndef HAVE_X86_ADAPT
    if (!knobs.empty())
    {
        Log::fatal() << "lo2s was built without support for x86_adapt; "
                        "cannot request x86_adapt knobs.\n";
        std::exit(EXIT_FAILURE);
    }
#endif
}

void to_json(nlohmann::json& j, const X86AdaptConfig& config)
{
    j = nlohmann::json({ { "enabled", config.enabled }, { "knobs", config.knobs } });
}
} // namespace lo2s
