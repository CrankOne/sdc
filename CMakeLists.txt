# SDC - A self-descriptive calibration data format library.
# Copyright (C) 2022  Renat R. Dusaev <renat.dusaev@cern.ch>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required (VERSION 3.9)

include (cmake/version.cmake)
get_pkg_version (SDC)
set (SDC_VERSION ${SDC_VERSION_MAJOR}.${SDC_VERSION_MINOR})

#
# General Project Features
project (sdc VERSION ${SDC_VERSION})

#
# Options
option (BUILD_TESTS "Enables ${CMAKE_PROJECT_NAME}-tests (requires gtest)" OFF)
option (COVERAGE "Enables ${CMAKE_PROJECT_NAME}-tests-coverage target" OFF)

#
# Dependencies
find_package(PkgConfig)
find_package(GTest QUIET)
find_package(ROOT QUIET COMPONENTS MathCore)

#
# Library
add_compile_options (-Wall)

set (sdc_LIB sdc)
add_library (${sdc_LIB} src/sdc.cc)
target_include_directories (${sdc_LIB}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
    )
# Force -fPIC or similar, to prevent clashes in applications that need it
set_property(TARGET ${sdc_LIB} PROPERTY POSITION_INDEPENDENT_CODE ON)
if (ROOT_FOUND)
    target_include_directories (${sdc_LIB} SYSTEM PUBLIC ${ROOT_INCLUDE_DIRS} )
    target_link_libraries (${sdc_LIB} PUBLIC ${ROOT_LIBRARIES})
else (ROOT_FOUND)
    message (STATUS "ROOT is not found; TFormula parsing is not supported.")
endif (ROOT_FOUND)
set_target_properties (${sdc_LIB} PROPERTIES VERSION ${SDC_VERSION}
        SOVERSION ${SDC_VERSION})

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/include/sdc.hh.in
                ${CMAKE_CURRENT_BINARY_DIR}/include/sdc.hh
                @ONLY)

#
# Tests
if (BUILD_TESTS)
    #if (CMAKE_COMPILER_IS_GNUCXX)
    # Discovery test
    add_executable (sdc-discover tests/discover.cc)
    target_link_libraries (sdc-discover ${sdc_LIB})

    add_executable (sdc-inspect tests/inspect-test.cc)
    target_link_libraries (sdc-inspect ${sdc_LIB})

    add_executable (sdc-example-1 tests/tutorial/main-1.cc)
    target_link_libraries (sdc-example-1 ${sdc_LIB})

    if (COVERAGE)
        set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
        include (CodeCoverage)
        set(COVERAGE_EXCLUDES "gtest/*")
        append_coverage_compiler_flags ()
        setup_target_for_coverage_gcovr_html (NAME sdc-discover-coverage
                                        EXECUTABLE sdc-discover
                                        BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
                                        EXCLUDE "tests/*;build/*")
    endif (COVERAGE)
    # Unit tests
    if (GTEST_FOUND)
        set (sdc_tests_SOURCES
                tests/sdc.cc
                tests/sdc-util.test.cc
                tests/sdc-grammar.test.cc
                tests/sdc-validity-range.test.cc
                tests/sdc-incremental-load.test.cc)
        set (sdc_UNITTESTS ${CMAKE_PROJECT_NAME}-tests)
        add_executable (${sdc_UNITTESTS} ${sdc_tests_SOURCES})
        target_include_directories (${sdc_UNITTESTS} PUBLIC include
            ${CMAKE_CURRENT_BINARY_DIR}/include SYSTEM ${GTEST_INCLUDE_DIRS})
        # Workaround for CMake bug found in some versions in 2012-2104, see:
        #   - https://stackoverflow.com/a/29871891/1734499
        set (THREADS_PREFER_PTHREAD_FLAG ON)
        find_package (Threads REQUIRED)
        #target_link_libraries( ${na64_UNITTESTS} PRIVATE Threads::Threads )
        target_link_libraries (${sdc_UNITTESTS} ${GTEST_BOTH_LIBRARIES} Threads::Threads ${sdc_LIB})

        if (COVERAGE)
            setup_target_for_coverage_gcovr_html (NAME ${sdc_UNITTESTS}-coverage
                                                EXECUTABLE ${sdc_UNITTESTS}
                                                BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
                                                EXCLUDE "tests/*;build/*")
        endif (COVERAGE)
        # (no install target for unit tests)
    else ()
        message (STATUS "gtest not found; unit tests won't be built.")
    endif ()
else (BUILD_TESTS)
    message (STATUS "Tests disabled")
endif (BUILD_TESTS)

# pkg-config, TODO
#get_target_property(USER_APP_CXX_FLAGS sdc COMPILE_OPTIONS)
#configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/sdc.pc.in
#                ${CMAKE_CURRENT_BINARY_DIR}/sdc.pc
#                @ONLY )
#install( FILES ${CMAKE_CURRENT_BINARY_DIR}/sdc.pc DESTINATION share/pkgconfig/)

#
# (Un)install
set_target_properties (${sdc_LIB} PROPERTIES PUBLIC_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/include/sdc.hh;include/sdc-base.hh")
include (GNUInstallDirs)
install (TARGETS ${sdc_LIB}
    EXPORT ${CMAKE_PROJECT_NAME}Targets
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
       
# Uninstall target (leaves empty dirs)
add_custom_target (uninstall
        "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/uninstall.cmake"
    )

# Usual CMake config export hell
install (EXPORT ${CMAKE_PROJECT_NAME}Targets
    FILE ${CMAKE_PROJECT_NAME}Targets.cmake
    DESTINATION lib/cmake//${CMAKE_PROJECT_NAME}
    )
include (CMakePackageConfigHelpers)
configure_package_config_file (${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "lib/cmake/${CMAKE_PROJECT_NAME}"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )
write_basic_package_version_file (
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${SDC_VERSION_MAJOR}.${SDC_VERSION_MINOR}"
    COMPATIBILITY AnyNewerVersion
    )
install (FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
    DESTINATION lib/cmake/${CMAKE_PROJECT_NAME}
    )
# Permits using SDC from build dir (without installing)
export(EXPORT ${CMAKE_PROJECT_NAME}Targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Targets.cmake"
    )


# CPack aux (packaging)
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${SDC_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${SDC_VERSION_MINOR}")
set(CPACK_SOURCE_GENERATOR "TBZ2")
include(CPack)

