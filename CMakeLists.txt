cmake_minimum_required( VERSION 3.13 )
#                                                      ________________________
# ___________________________________________________/ General Project Features
project( sdc )
set( SDC_VERSION_MAJOR 0 )
set( SDC_VERSION_MINOR 1 )
set( SDC_VERSION ${SDC_VERSION_MAJOR}.${SDC_VERSION_MINOR} )

option( COVERAGE "Enables ${CMAKE_PROJECT_NAME}-tests-coverage target (dev)"
    OFF )

find_package(PkgConfig)
find_package(GTest QUIET)
find_package(ROOT QUIET COMPONENTS MathCore)

add_compile_options( -Wall )

set( sdc_LIB sdc )
add_library(${sdc_LIB} SHARED src/sdc.cc)
target_include_directories(${sdc_LIB}
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
set_target_properties(${na64sdc_LIB} PROPERTIES VERSION ${NA64DP_VERSION}
                       SOVERSION ${NA64DP_VERSION_MAJOR}
                       #INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib/na64
                       )

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/include/sdc.hh.in
                ${CMAKE_CURRENT_BINARY_DIR}/include/sdc.hh
                @ONLY )
#install( FILES ${CMAKE_CURRENT_BINARY_DIR}/include/sdc.hh DESTINATION include/sdc)

#                                                              ________________
# ___________________________________________________________/ Discovery tests
add_executable( sdc-discover tests/discover.cc )
target_link_libraries( sdc-discover )

#                                                                   ___________
# ________________________________________________________________/ Unit tests
if( GTEST_FOUND )
    set( sdc_tests_SOURCES tests/sdc.cc )
    set( sdc_UNITTESTS ${CMAKE_PROJECT_NAME}-tests )
    add_executable( ${sdc_UNITTESTS} ${sdc_tests_SOURCES} )
    target_include_directories( ${sdc_UNITTESTS} PUBLIC include SYSTEM ${GTEST_INCLUDE_DIRS} )
    # Workaround for CMake bug found in some versions in 2012-2104, see:
    #   - https://stackoverflow.com/a/29871891/1734499
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    #target_link_libraries( ${na64_UNITTESTS} PRIVATE Threads::Threads )
    target_link_libraries( ${sdc_UNITTESTS} ${GTEST_BOTH_LIBRARIES} Threads::Threads )

    if(COVERAGE)
        set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/presets/cmake)
        if(CMAKE_COMPILER_IS_GNUCXX)
            include(CodeCoverage)
            append_coverage_compiler_flags()
            setup_target_for_coverage_gcovr_html(NAME ${na64_UNITTESTS}-coverage
                                                 EXECUTABLE ${na64_UNITTESTS}
                                                 BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
                                                 EXCLUDE "tests/*"
                                                         )
        endif()
    endif(COVERAGE)
    # (no install target for unit tests)
else()
    message(WARNING "gtest not found; unit tests won't be built.")
endif()

# pkg-config
#get_target_property(USER_APP_CXX_FLAGS sdc COMPILE_OPTIONS)
#configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/sdc.pc.in
#                ${CMAKE_CURRENT_BINARY_DIR}/sdc.pc
#                @ONLY )
#install( FILES ${CMAKE_CURRENT_BINARY_DIR}/sdc.pc DESTINATION share/pkgconfig/)

#                                                                  ____________
# _______________________________________________________________/ (Un)install
#
# Install include directory
# NOTE: CMake, generally, offers few ways to do this and one used here seems
# not to be a Most Modern one. But it is neat enough comparing with Most Modern
# (see, for instance https://discourse.cmake.org/t/installing-headers-the-modern-way-regurgitated-and-revisited/3238/2 )
# and suits our needs well enough.
install( DIRECTORY include/ # source directory
         DESTINATION include/sdc # target directory
         FILES_MATCHING PATTERN "*.h" PATTERN "*.hh" PATTERN "*.tcc" # select header files
       )
# Uninstall target (TODO: leaves empty dirs)
add_custom_target(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_SOURCE_DIR}/extensions/cmake/uninstall.cmake"
)

