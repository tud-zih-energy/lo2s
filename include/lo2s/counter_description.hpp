#pragma once

#include <otf2xx/common.hpp>
#include <string>

// Compared to EventAttr, this contains
// just interface agnostic meta-information
class CounterDescription
{

public:
    CounterDescription(std::string name)
    : name_(name), desc_(name), mode_(otf2::common::metric_mode::accumulated_last),
      type_(otf2::common::type::Double)
    {
    }

    CounterDescription(std::string name, std::string unit)
    : name_(name), desc_(name), unit_(unit), mode_(otf2::common::metric_mode::accumulated_last),
      type_(otf2::common::type::Double)
    {
    }

    CounterDescription(std::string name, std::string desc, std::string unit,
                       otf2::common::metric_mode mode, otf2::common::type type)
    : name_(name), desc_(desc), unit_(unit), mode_(mode), type_(type)
    {
    }

    const std::string& name() const
    {
        return name_;
    }

private:
    std::string name_;
    std::string desc_;
    std::string unit_ = "#";
    otf2::common::metric_mode mode_;
    otf2::common::type type_;
};
