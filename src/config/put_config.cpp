#include <lo2s/config/put_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

namespace lo2s
{
ProgramUnderTestConfig::ProgramUnderTestConfig(nitro::options::arguments& arguments)
: as_user(arguments), command(arguments.positionals())
{
    use_python = arguments.given("python");
}

void ProgramUnderTestConfig::add_parser(nitro::options::parser& parser)
{
    parser.accept_positionals();
    parser.positional_metavar("COMMAND");

    auto& put_options = parser.group("Program-under-test options");
    put_options.toggle("python", "Control Python perf recording support")
        .allow_reverse()
        .default_value(true);

    AsUserConfig::add_parser(parser);
}

void to_json(nlohmann::json& j, const ProgramUnderTestConfig& config)
{
    j = nlohmann::json({ { "as_user", config.as_user },
                         { "command", config.command },
                         { "use_python", config.use_python } });
}
} // namespace lo2s
