# Unit tests (mirrors caQtDM_UnitTests.pro and unitTests.pri).
# All tests define UNIT_TESTING; the ones testing caqtdm_lib compile
# caqtdm_lib.cpp themselves because under UNIT_TESTING the class is renamed
# to CaQtDM_Lib_TEST, which the prebuilt library does not export.

add_library(caqtdm_test_common INTERFACE)
target_compile_definitions(caqtdm_test_common INTERFACE
    UNIT_TESTING
    BUILDVERSION=\"UNITTEST\")
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_compile_definitions(caqtdm_test_common INTERFACE QT_DISABLE_DEPRECATED_BEFORE=0x050000)
endif()
target_link_libraries(caqtdm_test_common INTERFACE ${CAQTDM_QT_PACKAGE}::Test)
if(TARGET ${CAQTDM_QT_PACKAGE}::X11Extras)
    target_link_libraries(caqtdm_test_common INTERFACE ${CAQTDM_QT_PACKAGE}::X11Extras X11::X11)
endif()
if(MSVC)
    target_compile_options(caqtdm_test_common INTERFACE /wd4005)
endif()

set(_tests_src "${PROJECT_SOURCE_DIR}/caQtDM_QtControls/src")
set(_lib_src "${PROJECT_SOURCE_DIR}/caQtDM_Lib/src")
set(_viewer_src "${PROJECT_SOURCE_DIR}/caQtDM_Viewer/src")
set(_plugins_root "${PROJECT_SOURCE_DIR}/caQtDM_Plugins")

# --------------------------------------------------------------------------------------------------
# tst_viewer: logging system tests (no GUI)
# --------------------------------------------------------------------------------------------------
add_executable(tst_viewer
    tst_viewer/tst_viewer.cpp
    tst_viewer/tst_logging/tst_consoleloghandler.cpp
    tst_viewer/tst_logging/tst_fileloghandler.cpp
    tst_viewer/tst_logging/tst_generalloghandler.cpp
    tst_viewer/tst_logging/tst_logstashloghandler.cpp
    "${_viewer_src}/logging/consoleloghandler.cpp"
    "${_viewer_src}/logging/fileloghandler.cpp"
    "${_viewer_src}/logging/generalloghandler.cpp"
    "${_viewer_src}/logging/logstashloghandler.cpp"
    "${_viewer_src}/logging/abstractloghandler.h"
    "${_viewer_src}/logging/consoleloghandler.h"
    "${_viewer_src}/logging/fileloghandler.h"
    "${_viewer_src}/logging/generalloghandler.h"
    "${_viewer_src}/logging/logstashloghandler.h"
    tst_viewer/tst_logging/tst_consoleloghandler.h
    tst_viewer/tst_logging/tst_fileloghandler.h
    tst_viewer/tst_logging/tst_generalloghandler.h
    tst_viewer/tst_logging/tst_logstashloghandler.h)
if(UNIX)
    list(APPEND _tst_viewer_extra
        "${_viewer_src}/logging/syslogloghandler.cpp"
        "${_viewer_src}/logging/syslogloghandler.h")
    target_sources(tst_viewer PRIVATE ${_tst_viewer_extra})
endif()
target_include_directories(tst_viewer PRIVATE "${_viewer_src}/logging")
target_link_libraries(tst_viewer PRIVATE caqtdm_test_common ${CAQTDM_QT_PACKAGE}::Network)

# --------------------------------------------------------------------------------------------------
# tst_qtcontrols: widget tests (pvdialog compiled in, links qtcontrols)
# --------------------------------------------------------------------------------------------------
add_executable(tst_qtcontrols
    tst_qtcontrols/tst_qtcontrols.cpp
    tst_qtcontrols/tst_pvdialog.cpp
    tst_qtcontrols/tst_gensoftpv.cpp
    tst_qtcontrols/tst_canumeric.cpp
    tst_qtcontrols/tst_caspinbox.cpp
    tst_qtcontrols/tst_caapplynumeric.cpp
    tst_qtcontrols/tst_caimage.cpp
    tst_qtcontrols/tst_alhparser.cpp
    tst_qtcontrols/tst_alarmtree_logic.cpp
    "${_tests_src}/pvdialog.cpp"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/alhParserSrc/alhmodel.cpp"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/alhParserSrc/alhconfigparser.cpp"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/alhParserSrc/alhuigenerator.cpp"
    tst_qtcontrols/tst_alh_fixtures.qrc
    tst_qtcontrols/tst_pvdialog.h
    tst_qtcontrols/tst_gensoftpv.h
    tst_qtcontrols/tst_canumeric.h
    tst_qtcontrols/tst_caspinbox.h
    tst_qtcontrols/tst_caapplynumeric.h
    tst_qtcontrols/tst_caimage.h
    tst_qtcontrols/tst_alhparser.h
    tst_qtcontrols/tst_alarmtree_logic.h
    tst_qtcontrols/tst_numeric_suite.h
    tst_qtcontrols/fakeformwindow.h
    "${_tests_src}/pvdialog.h")
target_include_directories(tst_qtcontrols PRIVATE
    "${_tests_src}"
    "${_lib_src}"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/alhParserSrc")
target_link_libraries(tst_qtcontrols PRIVATE
    caqtdm_test_common
    qtcontrols
    ${CAQTDM_QT_PACKAGE}::Network
    ${CAQTDM_QT_PACKAGE}::Gui
    ${CAQTDM_QT_PACKAGE}::Widgets
    ${CAQTDM_QT_PACKAGE}::Designer)

# --------------------------------------------------------------------------------------------------
# tst_lib: dispatcher tests (caqtdm_lib.cpp + internal plugin compiled in)
# --------------------------------------------------------------------------------------------------
add_executable(tst_lib
    tst_lib/tst_lib.cpp
    tst_lib/tst_caqtdm_lib.cpp
    "${_lib_src}/caqtdm_lib.cpp"
    "${_plugins_root}/internal/internal_channel.cpp"
    "${_plugins_root}/internal/internal_plugin.cpp"
    tst_lib/tst_caqtdm_lib.h
    tst_lib/fakefileopenwindow.h
    "${_lib_src}/caqtdm_lib.h"
    "${_plugins_root}/internal/internal_channel.h"
    "${_plugins_root}/internal/internal_plugin.h"
    "${_viewer_src}/main.ui")
set_target_properties(tst_lib PROPERTIES AUTOUIC_SEARCH_PATHS "${_viewer_src}")
target_include_directories(tst_lib PRIVATE
    "${_tests_src}"
    "${PROJECT_SOURCE_DIR}/caQtDM_Lib"
    "${_lib_src}"
    "${_plugins_root}"
    "${_plugins_root}/internal"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/adlParserSrc"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/edlParserSrc")
target_link_libraries(tst_lib PRIVATE
    caqtdm_test_common
    caQtDM_Lib
    qtcontrols
    Epics::ca
    Epics::Com
    ${CAQTDM_QT_PACKAGE}::Network
    ${CAQTDM_QT_PACKAGE}::Gui
    ${CAQTDM_QT_PACKAGE}::Widgets
    ${CAQTDM_QT_PACKAGE}::Designer
    ${CAQTDM_QT_PACKAGE}::UiTools
    ${CAQTDM_QT_PACKAGE}::PrintSupport)

# --------------------------------------------------------------------------------------------------
# tst_viewer_input: viewer input handling (viewer sources + caqtdm_lib compiled in)
# --------------------------------------------------------------------------------------------------
add_executable(tst_viewer_input
    tst_viewer_input/tst_viewer_input.cpp
    tst_viewer_input/tst_envfilter.cpp
    tst_viewer_input/tst_networkaccess.cpp
    "${_viewer_src}/fileopenwindow.cpp"
    "${_viewer_src}/messagebox.cpp"
    "${_viewer_src}/configDialog.cpp"
    "${_tests_src}/networkaccess.cpp"
    "${_lib_src}/caqtdm_lib.cpp"
    tst_viewer_input/tst_envfilter.h
    tst_viewer_input/tst_networkaccess.h
    helpers/fakenetworkaccessmanager.h
    helpers/fakenetworkreply.h
    "${_viewer_src}/fileopenwindow.h"
    "${_viewer_src}/messagebox.h"
    "${_viewer_src}/configDialog.h"
    "${_viewer_src}/loggingcategories.h"
    "${_tests_src}/networkaccess.h"
    "${_lib_src}/caqtdm_lib.h"
    "${_viewer_src}/main.ui")
set_target_properties(tst_viewer_input PROPERTIES AUTOUIC_SEARCH_PATHS "${_viewer_src}")
target_include_directories(tst_viewer_input PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/helpers"
    "${_viewer_src}"
    "${_tests_src}"
    "${PROJECT_SOURCE_DIR}/caQtDM_Lib"
    "${_lib_src}"
    "${_plugins_root}"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/adlParserSrc"
    "${PROJECT_SOURCE_DIR}/caQtDM_Parsers/edlParserSrc")
target_compile_definitions(tst_viewer_input PRIVATE
    SUPPORT=\"EPICS\"
    BUILDARCH=\"UNITTEST\"
    BUILDTIME=\"\"
    BUILDDATE=\"\")
target_link_libraries(tst_viewer_input PRIVATE
    caqtdm_test_common
    caQtDM_Lib
    qtcontrols
    Epics::ca
    Epics::Com
    ${CAQTDM_QT_PACKAGE}::Network
    ${CAQTDM_QT_PACKAGE}::Gui
    ${CAQTDM_QT_PACKAGE}::Widgets
    ${CAQTDM_QT_PACKAGE}::Designer
    ${CAQTDM_QT_PACKAGE}::UiTools
    ${CAQTDM_QT_PACKAGE}::PrintSupport)

# --------------------------------------------------------------------------------------------------
# Plugin tests
# --------------------------------------------------------------------------------------------------
add_executable(tst_internal
    tst_plugins/tst_internal/tst_internal.cpp
    tst_plugins/tst_internal/tst_internal_channel.cpp
    tst_plugins/tst_internal/tst_internal_plugin.cpp
    "${_plugins_root}/internal/internal_channel.cpp"
    "${_plugins_root}/internal/internal_plugin.cpp"
    tst_plugins/tst_internal/tst_internal_channel.h
    tst_plugins/tst_internal/tst_internal_plugin.h
    "${_plugins_root}/internal/internal_channel.h"
    "${_plugins_root}/internal/internal_plugin.h")
target_include_directories(tst_internal PRIVATE
    "${_tests_src}"
    "${_lib_src}"
    "${_plugins_root}"
    "${_plugins_root}/internal")
target_link_libraries(tst_internal PRIVATE
    caqtdm_test_common
    qtcontrols
    caQtDM_Lib
    Epics::ca
    Epics::Com
    ${CAQTDM_QT_PACKAGE}::Network
    ${CAQTDM_QT_PACKAGE}::Gui
    ${CAQTDM_QT_PACKAGE}::Concurrent
    ${CAQTDM_QT_PACKAGE}::Widgets
    ${CAQTDM_QT_PACKAGE}::Designer
    ${CAQTDM_QT_PACKAGE}::UiTools
    ${CAQTDM_QT_PACKAGE}::PrintSupport)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(tst_internal PRIVATE "LINKER:--disable-new-dtags")
endif()

if(CAQTDM_BUILD_GPS)
    add_executable(tst_gps
        tst_plugins/tst_gps/tst_gps.cpp
        "${_plugins_root}/gps/gps_plugin.cpp"
        "${_plugins_root}/gps/gps_plugin.h")
    target_include_directories(tst_gps PRIVATE "${_lib_src}" "${_plugins_root}" "${_plugins_root}/gps")
    target_link_libraries(tst_gps PRIVATE caqtdm_test_common qtcontrols caQtDM_Lib
        ${CAQTDM_QT_PACKAGE}::Positioning ${CAQTDM_QT_PACKAGE}::Widgets)
endif()

if(CAQTDM_HAVE_BSREAD)
    add_executable(tst_bsread
        tst_plugins/tst_bsread/tst_bsread.cpp
        tst_plugins/tst_bsread/tst_bsread_decode.cpp
        tst_plugins/tst_bsread/tst_bsread_dispatchercontrol.cpp
        "${_plugins_root}/bsread/bsread_dispatchercontrol.cpp"
        "${_plugins_root}/bsread/bsread_decode.cpp"
        tst_plugins/tst_bsread/fakenetworkreply.h
        tst_plugins/tst_bsread/tst_bsread_decode.h
        tst_plugins/tst_bsread/tst_bsread_dispatchercontrol.h
        "${_plugins_root}/bsread/bsread_dispatchercontrol.h"
        "${_plugins_root}/bsread/bsread_decode.h")
    target_include_directories(tst_bsread PRIVATE
        "${_tests_src}"
        "${_lib_src}"
        "${_plugins_root}"
        "${_plugins_root}/bsread"
        "${ZMQ_INCLUDE_DIR}")
    target_link_libraries(tst_bsread PRIVATE
        caqtdm_test_common
        qtcontrols
        caQtDM_Lib
        caqtdm::zmq
        Epics::ca
        Epics::Com
        ${CAQTDM_QT_PACKAGE}::Network
        ${CAQTDM_QT_PACKAGE}::Gui
        ${CAQTDM_QT_PACKAGE}::Concurrent
        ${CAQTDM_QT_PACKAGE}::Widgets
        ${CAQTDM_QT_PACKAGE}::Designer
        ${CAQTDM_QT_PACKAGE}::UiTools
        ${CAQTDM_QT_PACKAGE}::PrintSupport)
    # Windows links through the plugin import library, as the qmake test does.
    # Other platforms link the plugin binary directly.
    if(WIN32)
        target_link_libraries(tst_bsread PRIVATE bsread_Plugin)
    else()
        target_link_libraries(tst_bsread PRIVATE "$<TARGET_FILE:bsread_Plugin>")
    endif()
    add_dependencies(tst_bsread bsread_Plugin)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(tst_bsread PRIVATE "LINKER:--disable-new-dtags")
    endif()
endif()

# --------------------------------------------------------------------------------------------------
# CTest registration
# --------------------------------------------------------------------------------------------------
set(_all_tests tst_viewer tst_qtcontrols tst_lib tst_viewer_input tst_internal)
if(TARGET tst_gps)
    list(APPEND _all_tests tst_gps)
endif()
if(TARGET tst_bsread)
    list(APPEND _all_tests tst_bsread)
endif()

if(WIN32)
    # ctest's ENVIRONMENT property replaces the variable, and ';' inside a
    # value must be escaped — otherwise the PATH falls apart. Compose a
    # complete PATH: collect dirs, Qt bin, Qwt lib, EPICS bin, then whatever
    # PATH the configure shell had.
    get_filename_component(_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
    set(_path "${CAQTDM_COLLECT}\\;${CAQTDM_COLLECT}/designer\\;${CAQTDM_COLLECT}/controlsystems\\;${_qt_bin_dir}")
    if(TARGET Qwt::Qwt)
        get_target_property(_qwt_location Qwt::Qwt IMPORTED_LOCATION_RELEASE)
        if(NOT _qwt_location)
            get_target_property(_qwt_location Qwt::Qwt IMPORTED_LOCATION)
        endif()
        if(_qwt_location)
            get_filename_component(_qwt_dir "${_qwt_location}" DIRECTORY)
            string(APPEND _path "\\;${_qwt_dir}")
        endif()
        unset(_qwt_location)
    endif()
    if(CAQTDM_EPICS_HOST_ARCH)
        string(APPEND _path "\\;${CAQTDM_EPICS_BASE}/bin/${CAQTDM_EPICS_HOST_ARCH}")
    endif()
    if(CAQTDM_ZMQ_BIN)
        string(APPEND _path "\\;${CAQTDM_ZMQ_BIN}")
    endif()
    if(CAQTDM_PYTHON_LIB_DIR)
        string(APPEND _path "\\;${CAQTDM_PYTHON_LIB_DIR}")
    endif()
    # the configure shell's PATH must be escaped as well, or ctest splits the
    # ENVIRONMENT list at its semicolons and truncates PATH
    string(REPLACE ";" "\\;" _env_path "$ENV{PATH}")
    string(APPEND _path "\\;${_env_path}")
    set(_test_env "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;PATH=${_path}")
elseif(APPLE)
    set(_test_env "QT_QPA_PLATFORM=offscreen;DYLD_LIBRARY_PATH=${CAQTDM_COLLECT}:${CAQTDM_COLLECT}/designer:${CAQTDM_COLLECT}/controlsystems:${Epics_LIBRARY_DIR}:${Qwt_LIBRARY_DIR}:${CAQTDM_PYTHON_LIB_DIR};DYLD_FRAMEWORK_PATH=${Qwt_LIBRARY_DIR}")
else()
    set(_test_env "QT_QPA_PLATFORM=offscreen;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-data;LD_LIBRARY_PATH=${CAQTDM_COLLECT}:${CAQTDM_COLLECT}/designer:${CAQTDM_COLLECT}/controlsystems:${Epics_LIBRARY_DIR}:${Qwt_LIBRARY_DIR}:${CAQTDM_ZMQ_LIB_DIR}:${CAQTDM_PYTHON_LIB_DIR}")
endif()

foreach(_test ${_all_tests})
    set_target_properties(${_test} PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME ${_test} COMMAND ${_test})
    set_tests_properties(${_test} PROPERTIES
        ENVIRONMENT "${_test_env}")
endforeach()
set_tests_properties(tst_qtcontrols PROPERTIES
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/caQtDM_UnitTests/tst_qtcontrols")
if(TARGET tst_bsread)
    set_tests_properties(tst_bsread PROPERTIES TIMEOUT 120)
endif()

caqtdm_apply_rpath(tst_viewer)
caqtdm_apply_rpath(tst_qtcontrols)
caqtdm_apply_rpath(tst_lib)
caqtdm_apply_rpath(tst_viewer_input)
caqtdm_apply_rpath(tst_internal)
if(TARGET tst_gps)
    caqtdm_apply_rpath(tst_gps)
endif()
if(TARGET tst_bsread)
    caqtdm_apply_rpath(tst_bsread)
endif()
