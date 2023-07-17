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

#include <lo2s/metric/nvml/process_recorder.hpp>

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

ProcessRecorder::ProcessRecorder(trace::Trace& trace, Gpu gpu)
: PollMonitor(trace, "gpu " + std::to_string(gpu.as_int()) + " (" + gpu.name() + ")",
              config().read_interval),
  metric_class_(trace.metric_class()), gpu_(gpu)
{
    auto result = nvmlDeviceGetHandleByIndex(gpu.as_int(), &device_);

    if (NVML_SUCCESS != result)
    {

        Log::error() << "Failed to get handle for device: " << nvmlErrorString(result);
        throw_errno();
    }

    metric_class_.add_member(trace.metric_member(
        "Decoder Utilization", "GPU Decoder Utilization by this Process",
        otf2::common::metric_mode::absolute_point, otf2::common::type::Double, "%"));
    metric_class_.add_member(trace.metric_member(
        "Encoder Utilization", "GPU Encoder Utilization by this Process",
        otf2::common::metric_mode::absolute_point, otf2::common::type::Double, "%"));
    metric_class_.add_member(trace.metric_member(
        "Memory Utilization", "GPU Memory Utilization by this Process",
        otf2::common::metric_mode::absolute_point, otf2::common::type::Double, "%"));
    metric_class_.add_member(trace.metric_member(
        "SM Utilization", "GPU SM Utilization by this Process",
        otf2::common::metric_mode::absolute_point, otf2::common::type::Double, "%"));
}

void ProcessRecorder::monitor([[maybe_unused]] int fd)
{
    auto now = time::now();

    unsigned int samples_count;

    nvmlDeviceGetProcessUtilization(device_, NULL, &samples_count, lastSeenTimeStamp);

    auto samples = std::make_unique<nvmlProcessUtilizationSample_t[]>(samples_count);

    auto result =
        nvmlDeviceGetProcessUtilization(device_, samples.get(), &samples_count, lastSeenTimeStamp);

    // NVML_ERROR_NOT_FOUND means, there are no samples since last query
    if (NVML_SUCCESS != result && result != NVML_ERROR_NOT_FOUND)
    {
        Log::error() << "Failed to get utilization for device: " << nvmlErrorString(result);
        throw_errno();
    }

    for (unsigned int i = 0; i < samples_count; i++)
    {
        const auto& sample = samples[i];

        auto process = Process(sample.pid);

        if (process_writers_.count(sample.pid) == 0)
        {
            process_writers_.try_emplace(sample.pid, &trace_.metric_writer(gpu_, process));
        }

        const auto& scope = trace_.registry().get<otf2::definition::location_group>(
            trace::ByGpuProcess(gpu_, process));

        const auto& instance = trace_.registry().emplace<otf2::definition::metric_instance>(
            trace::ByGpuProcess(gpu_, process), metric_class_,
            process_writers_.at(sample.pid)->location(), scope);

        otf2::event::metric event(now, instance);

        event.raw_values()[0] = double(sample.decUtil);
        event.raw_values()[1] = double(sample.encUtil);
        event.raw_values()[2] = double(sample.memUtil);
        event.raw_values()[3] = double(sample.smUtil);

        lastSeenTimeStamp = sample.timeStamp;

        // write event to archive
        process_writers_.at(sample.pid)->write(event);
    }
}

} // namespace nvml
} // namespace metric
} // namespace lo2s
