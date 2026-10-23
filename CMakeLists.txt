cmake_minimum_required(VERSION 3.24)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(caQtDMVersion)

project(caQtDM VERSION ${CAQTDM_VERSION_BASE} LANGUAGES C CXX)
set(PROJECT_VERSION_STR "${CAQTDM_VERSION_STR}")

# Do not export inlined/template symbols from caQtDM's DSOes, so they can't be
# interposed over libQt6Core's copies (e.g. QtPrivate::QPodArrayOps<double>::emplace)
# and corrupt Qwt's scale construction on FreeBSD (maybe also other platforms). Same flag Qt itself is built with.
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

include(caQtDMConfig)
include(caQtDMHelpers)

add_subdirectory(caQtDM_Parsers)
add_subdirectory(caQtDM_QtControls)
add_subdirectory(caQtDM_Lib)
add_subdirectory(caQtDM_Plugins)
add_subdirectory(caQtDM_Viewer)

if(CAQTDM_WITH_TESTS AND NOT CAQTDM_MOBILE)
    enable_testing()
    add_subdirectory(caQtDM_UnitTests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES
    DESCRIPTION "--- caQtDM ---")
