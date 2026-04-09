#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)


rm -rf test_trace

./lo2s --no-instruction-sampling --output-trace test_trace --accel nvidia -- ./dummy_gpu_events

if otf2-print test_trace/traces.otf2 | grep "foobar" >/dev/null; then
	exit 0
else
	echo "Trace did not contain the \"foobar\" GPU event!"
	exit 1
fi
