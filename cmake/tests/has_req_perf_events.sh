#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

if ! command -v perf; then
	exit -1
fi

events=$(perf list)
# lo2s uses either
if ! (echo $events | grep -q instructions && echo $events | grep -q cpu-cycles); then
	exit -1
fi

exit 0
