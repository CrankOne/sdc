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
set (SDC_VERSION ${SDC_VERSION_MAJOR}.${SDC_VERSION_MINOR}.${SDC_VERSION_TWEAK})

#
# General Project Features
project (sdc VERSION ${SDC_VERSION})

#
# Options
option (BUILD_TESTS "Enables ${CMAKE_PROJECT_NAME}-tests (requires gtest)" OFF)
option (COVERAGE "Enables ${CMAKE_PROJECT_NAME}-tests-coverage target" OFF)
set(SDC_SQL_SCRIPTS_PATH_ENVVAR "SDC_SQL_SCRIPTS_PATH")

#
# Dependencies
find_package(PkgConfig)
find_package(GTest QUIET)
find_package(ROOT QUIET COMPONENTS MathCore)

# For SQLite3 DB index and or cache
#find_package(SQlite3 QUIET)
# -- XXX -- XXX -- XXX ---
# FindSQLite3.cmake can be unavailable on the system (e.g. on Debian-based
# ones). This is the literal copy of the FindSQLite3.cmake found in modern
# CMake distributions. Will be deleted in future.
#if(NOT SQLite3_FOUND)
#    find_package(PkgConfig REQUIRED)
#    if(PKG_CONFIG_FOUND)
#        pkg_check_modules(PC_SQLite3 QUIET sqlite3)
#        # Look for the necessary header
#    find_path(SQLite3_INCLUDE_DIR NAMES sqlite3.h
#      HINTS
#        ${PC_SQLite3_INCLUDE_DIRS}
#    )
#    mark_as_advanced(SQLite3_INCLUDE_DIR)
#    # Look for the necessary library
#    find_library(SQLite3_LIBRARY NAMES sqlite3 sqlite
#      HINTS
#        ${PC_SQLite3_LIBRARY_DIRS}
#    )
#    mark_as_advanced(SQLite3_LIBRARY)
#    # Extract version information from the header file
#    if(SQLite3_INCLUDE_DIR)
#        file(STRINGS ${SQLite3_INCLUDE_DIR}/sqlite3.h _ver_line
#             REGEX "^#define SQLITE_VERSION  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
#             LIMIT_COUNT 1)
#        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
#               SQLite3_VERSION "${_ver_line}")
#        unset(_ver_line)
#    endif()
#    include(FindPackageHandleStandardArgs)
#    find_package_handle_standard_args(SQLite3
#        REQUIRED_VARS SQLite3_LIBRARY SQLite3_INCLUDE_DIR
#        VERSION_VAR SQLite3_VERSION)
#    # Create the imported target
#    if(SQLite3_FOUND)
#        set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
#        set(SQLite3_LIBRARIES ${SQLite3_LIBRARY})
#        if(NOT TARGET SQLite::SQLite3)
#            add_library(SQLite::SQLite3 UNKNOWN IMPORTED)
#            set_target_properties(SQLite::SQLite3 PROPERTIES
#                IMPORTED_LOCATION             "${SQLite3_LIBRARY}"
#                INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIR}")
#        endif()
#    endif()
#    endif(PKG_CONFIG_FOUND)
#endif(NOT SQLite3_FOUND)
# -- XXX -- XXX -- XXX ---

# To compress data in cache
find_package(Snappy QUIET)  # SNAPPY_FOUND
find_package(ZLIB QUIET)  # ZLIB_FOUND
# To obtain checksums
find_package(OpenSSL QUIET)  # OPENSSL_FOUND

#
# Library
add_compile_options (-Wall)

set (sdc_LIB sdc)

set (sdc_SOURCES src/sdc.cc src/sdc-fs.cc)
if(SQLite3_FOUND)
    list(APPEND sdc_SOURCES src/sdc-sqlite3.cc src/sdc-db.c src/sdc-sql.cc)
endif(SQLite3_FOUND)

add_library (${sdc_LIB} ${sdc_SOURCES})
target_include_directories (${sdc_LIB}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
    )
if(SQLite3_FOUND)
    target_link_libraries(${sdc_LIB} PUBLIC SQLite::SQLite3)
endif(SQLite3_FOUND)
if(SNAPPY_FOUND)
    target_include_directories(${sdc_LIB} ${SNAPPY_INCLUDE_DIR})
    target_link_libraries(${sdc_LIB} PRIVATE ${SNAPPY_LIBRARY})
endif(SNAPPY_FOUND)
if(ZLIB_FOUND)
    target_link_libraries(${sdc_LIB} PRIVATE ${ZLIB_LIBRARIES})
endif(ZLIB_FOUND)
if(OPENSSL_FOUND)
    target_link_libraries(${sdc_LIB} PRIVATE ${OPENSSL_LIBRARIES})
endif(OPENSSL_FOUND)
# Force -fPIC or similar, to prevent clashes in applications that need it
set_property(TARGET ${sdc_LIB} PROPERTY POSITION_INDEPENDENT_CODE ON)
if (ROOT_FOUND)
    target_include_directories (${sdc_LIB} SYSTEM PUBLIC ${ROOT_INCLUDE_DIRS} )
    target_link_libraries (${sdc_LIB} PUBLIC ${ROOT_LIBRARIES})
else (ROOT_FOUND)
    message (STATUS "ROOT is not found; TFormula parsing is not supported.")
endif (ROOT_FOUND)
# for soversion we omit tweak number
set_target_properties (${sdc_LIB} PROPERTIES VERSION ${SDC_VERSION_MAJOR}.${SDC_VERSION_MINOR}
        SOVERSION ${SDC_VERSION_MAJOR}.${SDC_VERSION_MINOR})

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/include/sdc-config.h.in
                ${CMAKE_CURRENT_SOURCE_DIR}/include/sdc-config.h
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
                tests/sdc-incremental-load.test.cc
                tests/sdc-index.test.cc
                )
        if(SQLite3_FOUND)
            list(APPEND sdc_tests_SOURCES tests/sdc-index-sqlite3.test.cc)
        endif(SQLite3_FOUND)
        set (sdc_UNITTESTS ${CMAKE_PROJECT_NAME}-tests)
        add_executable (${sdc_UNITTESTS} ${sdc_tests_SOURCES})
        target_include_directories (${sdc_UNITTESTS} PUBLIC include
            SYSTEM ${GTEST_INCLUDE_DIRS})
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
# omit tweak version
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
set(CPACK_PACKAGE_VERSION_PATCH "${SDC_VERSION_TWEAK}")
set(CPACK_SOURCE_GENERATOR "TBZ2")
include(CPack)

