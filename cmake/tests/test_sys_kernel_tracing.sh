# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

if test ! -r "/sys/kernel/tracing/events/syscalls/sys_enter_write/format"; then
	exit -1
else
	exit 0
fi
