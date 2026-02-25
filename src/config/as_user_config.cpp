#include <lo2s/config/as_user_config.hpp>

#include <nitro/options/arguments.hpp>
#include <nitro/options/parser.hpp>
#include <nlohmann/json.hpp>

namespace lo2s
{
AsUserConfig::AsUserConfig(nitro::options::arguments& arguments)
{
    drop_root = arguments.given("drop-root");

    if (arguments.provided("as-user"))
    {
        drop_root = true;
        user = arguments.get("as-user");
    }
}

void AsUserConfig::add_parser(nitro::options::parser& parser)
{
    auto& as_user_options = parser.group("User change options");
    as_user_options.toggle("drop-root", "Drop root privileges before launching COMMAND.")
        .short_name("u");

    as_user_options.option("as-user", "Launch the COMMAND as the user USERNAME.")
        .short_name("U")
        .metavar("USERNAME")
        .optional();
}

void to_json(nlohmann::json& j, const AsUserConfig& config)
{
    j = nlohmann::json({ { "drop_root", config.drop_root }, { "user", config.user } });
}
} // namespace lo2s
