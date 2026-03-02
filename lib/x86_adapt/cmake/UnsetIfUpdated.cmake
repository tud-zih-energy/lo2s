# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

macro(NitroUnsetIfUpdated UnsetVar WatchedVar)
    if(NOT ${WatchedVar} STREQUAL OLD_${WatchedVar})
        set(OLD_${WatchedVar} ${${WatchedVar}} CACHE INTERNAL "previous value of ${WatchedVar}")
        unset(${UnsetVar} CACHE)
    endif()
endmacro()
