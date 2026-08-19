cmake_minimum_required(VERSION 3.20)

project(AlongTrackPortableCore VERSION 1.0.0 LANGUAGES CXX)

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

if(MSVC)
    target_compile_options(alongtrack_portable_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(alongtrack_portable_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

include(CTest)
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
    if(MSVC)
        target_compile_options(alongtrack_portable_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(alongtrack_portable_core_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME alongtrack_portable_core COMMAND alongtrack_portable_core_tests)
    add_test(
        NAME alongtrack_portable_core_source_policy
        COMMAND ${CMAKE_COMMAND}
            -DATS_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests/check_source_policy.cmake
    )
endif()
