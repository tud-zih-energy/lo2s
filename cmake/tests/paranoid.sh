#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)

if [ $paranoid -gt $1 -a $UID != 0 ]; then
	exit -1
else
	exit 0
fi
