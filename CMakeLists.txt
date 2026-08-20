cmake_minimum_required(VERSION 3.20)

project(AlongTrackPortableCore VERSION 1.0.0 LANGUAGES CXX)

option(ALONGTRACK_WARNINGS_AS_ERRORS
    "Treat AlongTrackSimulator C++ warnings as errors"
    OFF
)
option(ALONGTRACK_BUILD_WAVEVORTEX_EXTENSION
    "Build the source-linked WaveVortexModel AlongTrack extension"
    OFF
)
set(ALONGTRACK_WAVEVORTEX_SOURCE_DIR "" CACHE PATH
    "WaveVortexModel source checkout used by the source-linked extension"
)

function(alongtrack_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(ALONGTRACK_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(ALONGTRACK_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(alongtrack_portable_core
    cpp/src/portable_core.cpp
)
add_library(AlongTrack::PortableCore ALIAS alongtrack_portable_core)

target_compile_features(alongtrack_portable_core PUBLIC cxx_std_17)
target_include_directories(alongtrack_portable_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(alongtrack_portable_core PROPERTIES
    CXX_EXTENSIONS OFF
)

alongtrack_enable_warnings(alongtrack_portable_core)

include(CTest)

if(ALONGTRACK_BUILD_WAVEVORTEX_EXTENSION)
    if(NOT TARGET WaveVortex::PortableRuntime)
        if(NOT ALONGTRACK_WAVEVORTEX_SOURCE_DIR)
            message(FATAL_ERROR
                "ALONGTRACK_WAVEVORTEX_SOURCE_DIR must name a WaveVortexModel source checkout")
        endif()
        if(NOT EXISTS
           "${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/CMakeLists.txt")
            message(FATAL_ERROR
                "ALONGTRACK_WAVEVORTEX_SOURCE_DIR has no PortableRuntime CMake project")
        endif()

        # WaveVortexModel remains a source-linked runtime dependency. Excluding
        # its standalone targets and test suite from the default build keeps
        # this target narrowly scoped to ATS #3.
        set(_alongtrack_saved_build_testing "${BUILD_TESTING}")
        set(BUILD_TESTING OFF)
        add_subdirectory(
            "${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}/PortableRuntime"
            "${CMAKE_CURRENT_BINARY_DIR}/wavevortex-portable-runtime"
            EXCLUDE_FROM_ALL
        )
        set(BUILD_TESTING "${_alongtrack_saved_build_testing}")
        unset(_alongtrack_saved_build_testing)
    endif()

    add_subdirectory(cpp/extensions/wavevortex)
endif()

if(BUILD_TESTING)
    add_executable(alongtrack_portable_core_tests
        cpp/tests/test_portable_core.cpp
    )
    target_link_libraries(alongtrack_portable_core_tests PRIVATE AlongTrack::PortableCore)
    target_compile_definitions(alongtrack_portable_core_tests PRIVATE
        ATS_MATLAB_REFERENCE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests/data/matlab_reference.csv"
    )
    target_compile_features(alongtrack_portable_core_tests PRIVATE cxx_std_17)
    set_target_properties(alongtrack_portable_core_tests PROPERTIES CXX_EXTENSIONS OFF)
    alongtrack_enable_warnings(alongtrack_portable_core_tests)
    add_test(NAME alongtrack_portable_core COMMAND alongtrack_portable_core_tests)
    add_test(
        NAME alongtrack_portable_core_source_policy
        COMMAND ${CMAKE_COMMAND}
            -DATS_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests/check_source_policy.cmake
    )
endif()
