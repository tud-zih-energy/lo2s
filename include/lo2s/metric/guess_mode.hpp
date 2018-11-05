#pragma once

#include <lo2s/log.hpp>

#include <otf2xx/common.hpp>

#include <string>

namespace lo2s
{
namespace metric
{

// Yup, this modifies name, deal with it.
inline otf2::common::metric_mode
guess_mode(std::string& name,
           otf2::common::metric_mode default_ = otf2::common::metric_mode::absolute_point)
{
    using otf2::common::metric_mode;
    auto pos = name.find("#");
    if (pos == std::string::npos)
    {
        return default_;
    }

    auto suffix = name.substr(pos + 1);
    name = name.substr(0, pos);

    if (suffix == "absolute_point" || suffix == "point" || suffix == "p")
    {
        return metric_mode::absolute_point;
    }
    if (suffix == "accumulated_last" || suffix == "last" || suffix == "l")
    {
        return metric_mode::accumulated_last;
    }
    if (suffix == "accumulated_start" || suffix == "accumulated")
    {
        return metric_mode::accumulated_start;
    }
    // TODO the rest
    Log::warn() << "Unknown metric mode modifier: " << suffix;
    return default_;
}
} // namespace metric
} // namespace lo2s
