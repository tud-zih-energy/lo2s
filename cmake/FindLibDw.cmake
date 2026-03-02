# SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/PkgConfigWrapper.cmake)
# Linking libdw statically isn't a great default because it produces warnings
option(LibDw_USE_STATIC_LIBS "Link libelf statically." OFF)

FindPkgConfigWrapper(LibDw libdw dw)
