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

#include <lo2s/metric/nvml/nvml_recorder.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/util.hpp>

#include <nitro/lang/enumerate.hpp>

#include <cstring>

namespace lo2s
{
namespace metric
{
namespace nvml
{


Recorder::Recorder(trace::Trace& trace)
: PollMonitor(trace, "NVML recorder", config().read_interval),
  otf2_writer_(trace.create_metric_writer(name())),
  metric_instance_(trace.metric_instance(trace.metric_class(), otf2_writer_.location(),
                                         trace.system_tree_root_node()))
{

    unsigned int device_count;

    // First initialize NVML library
    result = nvmlInit();

    if (NVML_SUCCESS != result){ 

        Log::error() << "Failed to initialize NVML: " << nvmlErrorString(result);
        throw_errno();
    }

    // Get number of GPUs
    result = nvmlDeviceGetCount(&device_count);

    if (NVML_SUCCESS != result){ 

        Log::error() << "Failed to query device count: " << nvmlErrorString(result);
        throw_errno();
    }

    Log::debug() << "Found " << device_count << " GPU(s)";

    // Get GPU handle and name
    
    char name[64];

    result = nvmlDeviceGetHandleByIndex(0, &device);

        if (NVML_SUCCESS != result){ 

            Log::error() << "Failed to get handle for device: " << nvmlErrorString(result);
            throw_errno();
        }

    result = nvmlDeviceGetName(device, name, sizeof(name)/sizeof(name[0]));

        if (NVML_SUCCESS != result){ 

            Log::error() << "Failed to get name for device: " << nvmlErrorString(result);
            throw_errno();
        }

    auto mc = otf2::definition::make_weak_ref(metric_instance_.metric_class());

    mc->add_member(trace.metric_member(std::string(name), "Power Usage",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "mW"));

    event_ = std::make_unique<otf2::event::metric>(otf2::chrono::genesis(), metric_instance_);
}

void Recorder::monitor([[maybe_unused]] int fd)
{
    // update timestamp
    event_->timestamp(time::now());

    // update event values with nvml data
    unsigned int power;
        
	result = nvmlDeviceGetPowerUsage(device, &power);

    if (NVML_SUCCESS != result){ 
        
        throw_errno();

    }

    event_->raw_values()[0] = double(power);

    // write event to archive
    otf2_writer_.write(*event_);
}

Recorder::~Recorder()
{
    result = nvmlShutdown();

    if (NVML_SUCCESS != result){

        Log::error() << "Failed to get handle for device: " << nvmlErrorString(result);
        throw_errno();
    }
}
} // namespace nvml
} // namespace metric
} // namespace lo2s
