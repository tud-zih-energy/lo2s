# SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
option(LibBpf_USE_STATIC_LIBS "Link LibBpf statically." ON)

include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigWrapper.cmake)

FindPkgConfigWrapper(LibBpf libbpf bpf)
