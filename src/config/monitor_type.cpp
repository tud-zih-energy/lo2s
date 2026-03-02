// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/config/monitor_type.hpp>

#include <nlohmann/json.hpp>

namespace lo2s
{
NLOHMANN_JSON_SERIALIZE_ENUM(MonitorType, { { MonitorType::PROCESS, "process" },
                                            { MonitorType::CPU_SET, "cpu_set" } })
}
