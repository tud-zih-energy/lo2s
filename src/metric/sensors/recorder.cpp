/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2022,
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

#include <lo2s/metric/sensors/recorder.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <nitro/lang/enumerate.hpp>

extern "C"
{
#include <sensors/sensors.h>
}

#include <cstring>

namespace lo2s
{
namespace metric
{
namespace sensors
{

static const char* get_unit(const sensors_feature* feature)
{
    switch (feature->type)
    {
    case SENSORS_FEATURE_IN:
        return "V";
    case SENSORS_FEATURE_FAN:
        return "RPM";
    case SENSORS_FEATURE_TEMP:
        return "°C";
    case SENSORS_FEATURE_POWER:
        return "W";
    case SENSORS_FEATURE_ENERGY:
        return "J";
    case SENSORS_FEATURE_CURR:
        return "A";
    case SENSORS_FEATURE_HUMIDITY:
        return "%";
    case SENSORS_FEATURE_VID:
        return "V";
    default:
        return feature->name;
    }
}

Recorder::Recorder(trace::Trace& trace)
: PollMonitor(trace, "Sensors recorder", config().read_interval),
  otf2_writer_(trace.create_metric_writer(name())),
  metric_instance_(trace.metric_instance(trace.metric_class(), otf2_writer_.location(),
                                         trace.system_tree_root_node()))
{
    sensors_init(nullptr);

    // Can I just leave a rant here? This freaking C API is the worst. Who, in gods name, came up
    // with that bullshit? "nr is an internally used variable". For fucks sake. (┛◉Д◉) ┛彡┻━┻

    int chip_id = 0;
    for (auto chip = sensors_get_detected_chips(nullptr, &chip_id); chip != nullptr;
         chip = sensors_get_detected_chips(nullptr, &chip_id))
    {
        int chip_name_size = sensors_snprintf_chip_name(nullptr, 0, chip);
        assert(chip_name_size > 0);

        std::string chip_name;
        chip_name.resize(chip_name_size);
        int res = sensors_snprintf_chip_name(chip_name.data(), chip_name.size() + 1, chip);

        if (res < 0)
        {
            Log::error() << "Failed to get chip name for: " << chip->path << ". Skipping.";
            continue;
        }

        int feature_id = 0;
        for (auto feature = sensors_get_features(chip, &feature_id); feature != nullptr;
             feature = sensors_get_features(chip, &feature_id))
        {
            std::unique_ptr<char, memory::MallocDelete<char>> label{ sensors_get_label(chip,
                                                                                       feature) };

            if (!label)
            {
                // SHUT THE FRIDGE! There's no std::strdup? (ﾉ °益°)ﾉ 彡 ┻━┻
                label.reset(::strdup(feature->name));
            }

            int sub_feature_id = 0;
            for (auto sub_feature = sensors_get_all_subfeatures(chip, feature, &sub_feature_id);
                 sub_feature != nullptr;
                 sub_feature = sensors_get_all_subfeatures(chip, feature, &sub_feature_id))
            {
                // Convention says, that the sub feature ending with "_input" is the measurement.
                // So naturally we ignore everything else. I don't give a fuck for min/max.
                auto name = std::string(sub_feature->name);
                if (name.size() <= 6 ||
                    name.substr(name.size() - 6) != "_input") // if only nitro had an "ends_with"
                                                              // or we could use c++20...
                    continue;

                std::stringstream sensor_name;
                sensor_name << chip_name << "/" << feature->name << " (" << label.get() << ")";
                Log::debug() << "Found sensor: " << sensor_name.str();

                items_.emplace_back(
                    std::make_pair<const void*, int>(chip, int(sub_feature->number)));
                auto mc = otf2::definition::make_weak_ref(metric_instance_.metric_class());

                mc->add_member(trace.metric_member(sensor_name.str(), label.get(),
                                                   otf2::common::metric_mode::absolute_point,
                                                   otf2::common::type::Double, get_unit(feature)));
            }
        }
    }

    event_ = std::make_unique<otf2::event::metric>(otf2::chrono::genesis(), metric_instance_);
}

void Recorder::monitor([[maybe_unused]] int fd)
{
    // update timestamp
    event_->timestamp(time::now());

    // update event values with sensors data
    for (const auto& index_items : nitro::lang::enumerate(items_))
    {
        double value;
        const auto [chip, subf] = index_items.value();
        int res = sensors_get_value(reinterpret_cast<const sensors_chip_name*>(chip), subf, &value);
        if (res < 0)
        {
            throw_errno();
        }

        auto i = index_items.index();
        event_->raw_values()[i] = value;
    }

    // write event to archive
    otf2_writer_.write(*event_);
}

Recorder::~Recorder()
{
    sensors_cleanup();
}
} // namespace sensors
} // namespace metric
} // namespace lo2s
