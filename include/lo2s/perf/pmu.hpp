/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2025,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <lo2s/perf/event_attr.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <string>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s
{

namespace perf
{
class PMU
{
public:
    PMU(std::string pmu_name) : pmu_name_(pmu_name)
    {

        pmu_path_ = std::filesystem::path("/sys/bus/event_source/devices") / pmu_name;

        if (!std::filesystem::exists(pmu_path_))
        {
            throw EventAttr::InvalidEvent("unknown PMU '" + pmu_name + "'");
        }
    }

    template <typename T>
    std::optional<T> try_read_file(const std::string& filename)
    {
        T val;
        std::ifstream stream(filename);
        stream >> val;
        if (stream.fail())
        {
            return {};
        }
        return val;
    }

    perf_type_id type()
    {
        auto type = try_read_file<std::underlying_type<perf_type_id>::type>(pmu_path_ / "type");
        return static_cast<perf_type_id>(type.value());
    }

    std::string name()
    {
        return pmu_name_;
    }

    double scale(std::string specific_event)
    {
        return try_read_file<double>(event_path(specific_event).replace_extension(".scale"))
            .value_or(1.0);
    }

    std::string unit(std::string specific_event)
    {
        return try_read_file<std::string>(event_path(specific_event).replace_extension(".unit"))
            .value_or("#");
    }

    std::filesystem::path event_path(std::string specific_event)
    {
        return pmu_path_ / "events" / specific_event;
    }

    struct perf_event_attr ev_from_name(std::string specific_event)
    {

        // read event configuration
        auto ev_cfg = try_read_file<std::string>(event_path(specific_event));
        if (!ev_cfg.has_value())
        {
            using namespace std::string_literals;
            throw EventAttr::InvalidEvent("unknown event '"s + specific_event + "' for PMU '"s +
                                          pmu_name_ + "'");
        }

        return ev_from_string(ev_cfg.value());
    }

    struct perf_event_attr ev_from_string(std::string ev_string)
    {
        struct perf_event_attr attr;
        memset(&attr, 0, sizeof(attr));

        // Parse event configuration from sysfs //

        /* Event configuration format:
         *   One or more terms with optional values, separated by ','.  (Taken from
         *   linux/Documentation/ABI/testing/sysfs-bus-event_source-devices-events):
         *
         *     <term>[=<value>][,<term>[=<value>]...]
         *
         *   Example (config for 'cpu/cache-misses' on an Intel Core i5-7200U):
         *
         *     event=0x2e,umask=0x41
         *
         *  */

        enum EVENT_CONFIG_REGEX_GROUPS
        {
            EC_WHOLE_MATCH,
            EC_TERM,
            EC_VALUE,
        };

        static const std::regex kv_regex(R"(([^=,]+)(?:=([^,]+))?)");

        Log::debug() << "parsing event configuration: " << ev_string;
        std::smatch kv_match;
        while (std::regex_search(ev_string, kv_match, kv_regex))
        {
            static const std::string default_value("0x1");

            const std::string& term = kv_match[EC_TERM];
            const std::string& value =
                (kv_match[EC_VALUE].length() != 0) ? kv_match[EC_VALUE] : default_value;

            auto format = try_read_file<std::string>(pmu_path_ / "format" / term);
            if (!format.has_value())
            {
                throw EventAttr::InvalidEvent("cannot read event format");
            }

            static_assert(sizeof(std::uint64_t) >= sizeof(unsigned long),
                          "May not convert from unsigned long to uint64_t!");

            std::uint64_t val = std::stol(value, nullptr, 0);
            Log::debug() << "parsing config assignment: " << term << " = " << std::hex
                         << std::showbase << val << std::dec << std::noshowbase;
            event_attr_update(attr, val, format.value());

            ev_string = kv_match.suffix();
        }
        return attr;
    }

    void event_attr_update(struct perf_event_attr& attr, std::uint64_t value,
                           const std::string& format)
    {
        // Parse config terms //

        /* Format:  <term>:<bitmask>
         *
         * We only assign the terms 'config' and 'config1'.
         *
         * */

        static constexpr auto npos = std::string::npos;
        const auto colon = format.find_first_of(':');
        if (colon == npos)
        {
            throw EventAttr::InvalidEvent("invalid format description: missing colon");
        }

        const auto target_config = format.substr(0, colon);
        const auto mask = parse_bitmask(format.substr(colon + 1));

        if (target_config == "config")
        {
            attr.config |= apply_mask(value, mask);
        }

        if (target_config == "config1")
        {
            attr.config1 |= apply_mask(value, mask);
        }
    }

    std::uint64_t apply_mask(std::uint64_t value, std::uint64_t mask)
    {
        std::uint64_t res = 0;
        for (int mask_bit = 0, value_bit = 0; mask_bit < 64; mask_bit++)
        {
            if (mask & (1ull << mask_bit))
            {
                res |= ((value >> value_bit) & (1ull << 0)) << mask_bit;
                value_bit++;
            }
        }
        return res;
    }

    std::uint64_t parse_bitmask(const std::string& format)
    {
        enum BITMASK_REGEX_GROUPS
        {
            BM_WHOLE_MATCH,
            BM_BEGIN,
            BM_END,
        };

        std::uint64_t mask = 0x0;

        static const std::regex bit_mask_regex(R"((\d+)?(?:-(\d+)))");
        const std::sregex_iterator end;
        for (std::sregex_iterator i = { format.begin(), format.end(), bit_mask_regex }; i != end;
             ++i)
        {
            const auto& match = *i;
            int start = std::stoi(match[BM_BEGIN]);
            int end = (match[BM_END].length() == 0) ? start : std::stoi(match[BM_END]);

            const auto len = (end + 1) - start;
            if (start < 0 || end > 63 || len > 64)
            {
                throw EventAttr::InvalidEvent("invalid config mask");
            }

            /* Set `len` bits and shift them to where they should start.
             * 4-bit example: format "1-3" produces mask 0b1110.
             *    start := 1, end := 3
             *    len  := 3 + 1 - 1 = 3
             *    bits := bit(3) - 1 = 0b1000 - 1 = 0b0111
             *    mask := 0b0111 << 1 = 0b1110
             * */

            // Shifting by 64 bits causes undefined behaviour, so in this case set
            // all bits by assigning the maximum possible value for std::uint64_t.
            const std::uint64_t bits =
                (len == 64) ? std::numeric_limits<std::uint64_t>::max() : (1ull << len) - 1;

            mask |= bits << start;
        }
        Log::debug() << std::showbase << std::hex << "config mask: " << format << " = " << mask
                     << std::dec << std::noshowbase;
        return mask;
    }

private:
    std::string pmu_name_;
    std::filesystem::path pmu_path_;
};

} // namespace perf
} // namespace lo2s
