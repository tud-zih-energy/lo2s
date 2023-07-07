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


Recorder::Recorder(trace::Trace& trace, Gpu gpu)
: PollMonitor(trace, "NVML recorder", config().read_interval),
  otf2_writer_(trace.create_metric_writer(name())),
  metric_instance_(trace.metric_instance(trace.metric_class(), otf2_writer_.location(),
                                         trace.system_tree_gpu_node(gpu)))
{

    result = nvmlInit();

    if (NVML_SUCCESS != result){ 

        Log::error() << "Failed to initialize NVML: " << nvmlErrorString(result);
        throw_errno();
    }

    result = nvmlDeviceGetHandleByIndex(gpu.as_int(), &device);

    if (NVML_SUCCESS != result){ 

        Log::error() << "Failed to get handle for device: " << nvmlErrorString(result);
        throw_errno();
    }

    auto mc = otf2::definition::make_weak_ref(metric_instance_.metric_class());

    mc->add_member(trace.metric_member("GPU Power Usage", "Total power consumption of this GPU",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "W"));
    mc->add_member(trace.metric_member("GPU Temperature", "Temperature of the GPU die",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "°C"));
    mc->add_member(trace.metric_member("GPU Fan Speed", "Percentage of maximum Fan Speed",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "%"));
    mc->add_member(trace.metric_member("GPU Graphics Clock", "Speed of Graphics Clock Domain",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "MHz"));
    mc->add_member(trace.metric_member("GPU SM Clock", "Speed of Streaming Multiprocessor Clock Domain",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "MHz"));
    mc->add_member(trace.metric_member("GPU Memory Clock", "Speed of Memory Clock Domain",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "MHz"));
    mc->add_member(trace.metric_member("GPU Video Clock", "Speed of Video Encoder/Decoder Clock Domain",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "MHz"));
    mc->add_member(trace.metric_member("GPU Utilization Rate", "Percentage of last sample period where kernels were executing",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "%"));
    mc->add_member(trace.metric_member("GPU Memory Utilization Rate", "Percentage of last sample period where memory was read/written",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "%"));
    mc->add_member(trace.metric_member("GPU PState", "Performance State of the GPU",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, ""));
    mc->add_member(trace.metric_member("NVML monitoring time", "time taken to get GPU metrics via nvml",
                                        otf2::common::metric_mode::absolute_point,
                                        otf2::common::type::Double, "ms"));

    event_ = std::make_unique<otf2::event::metric>(otf2::chrono::genesis(), metric_instance_);
}

void Recorder::monitor([[maybe_unused]] int fd)
{
    // update timestamp
    event_->timestamp(time::now());

    otf2::chrono::time_point start = time::now();

    // update event values with nvml data
    unsigned int power;
    unsigned int temp;
    unsigned int fan_speed;
    unsigned int g_clock;
    unsigned int sm_clock;
    unsigned int mem_clock;
    unsigned int vid_clock;
    nvmlUtilization_t utilization;
    nvmlPstates_t p_state;

        
	result = nvmlDeviceGetPowerUsage(device, &power);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp) : result);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetFanSpeed(device, &fan_speed) : result);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &g_clock) : result);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetClockInfo(device, NVML_CLOCK_SM, &sm_clock) : result);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetClockInfo(device, NVML_CLOCK_MEM, &mem_clock) : result);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetClockInfo(device, NVML_CLOCK_VIDEO, &vid_clock) : result);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetUtilizationRates(device, &utilization) : result);

    result = (NVML_SUCCESS == result ? nvmlDeviceGetPerformanceState(device, &p_state) : result);


    if (NVML_SUCCESS != result){ 
        
        throw_errno();

    }

    auto time_taken = time::now() - start;
    

    event_->raw_values()[0] = double(power/1000);
    event_->raw_values()[1] = double(temp);
    event_->raw_values()[2] = double(fan_speed);
    event_->raw_values()[3] = double(g_clock);
    event_->raw_values()[4] = double(sm_clock);
    event_->raw_values()[5] = double(mem_clock);
    event_->raw_values()[6] = double(vid_clock);
    event_->raw_values()[7] = double(utilization.gpu);
    event_->raw_values()[8] = double(utilization.memory);
    event_->raw_values()[9] = double(p_state);
    event_->raw_values()[10] = double(std::chrono::duration_cast<std::chrono::microseconds>(time_taken).count());

    // write event to archive
    otf2_writer_.write(*event_);
}

Recorder::~Recorder()
{
    result = nvmlShutdown();

    if (NVML_SUCCESS != result){

        Log::error() << "Failed to shutdown NVML: " << nvmlErrorString(result);
        throw_errno();
    }
}
} // namespace nvml
} // namespace metric
} // namespace lo2s
