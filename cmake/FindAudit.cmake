# SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later
include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigWrapper.cmake)

option(Audit_USE_STATIC_LIBS "Link libaudit statically" OFF)

FindPkgConfigWrapper(Audit audit audit)
