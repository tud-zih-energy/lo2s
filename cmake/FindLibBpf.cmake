# SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause
#
# SPDX-FileCopyrightText: (c) 2020, Andrii Nakryiko
# SPDX-FileCopyrightText: (c) 2026 Technische Universität Dresden
#
# Originally from: https://github.com/libbpf/libbpf-bootstrap

option(LibBpf_USE_STATIC_LIBS "Link LibBpf statically." ON)

include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigWrapper.cmake)

FindPkgConfigWrapper(LibBpf libbpf bpf)
