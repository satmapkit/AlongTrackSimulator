add_library(alongtrack_wavevortex_extension STATIC
    src/wavevortex_extension.cpp
)
add_library(AlongTrack::WaveVortexExtension ALIAS
    alongtrack_wavevortex_extension
)

target_compile_features(alongtrack_wavevortex_extension PUBLIC cxx_std_17)
target_include_directories(alongtrack_wavevortex_extension
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(alongtrack_wavevortex_extension
    PUBLIC
        AlongTrack::PortableCore
        WaveVortex::PortableRuntime
)
set_target_properties(alongtrack_wavevortex_extension PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
alongtrack_enable_warnings(alongtrack_wavevortex_extension)

if(BUILD_TESTING)
    add_executable(alongtrack_wavevortex_extension_tests
        tests/test_wavevortex_extension.cpp
    )
    target_compile_features(alongtrack_wavevortex_extension_tests
        PRIVATE cxx_std_17)
    target_link_libraries(alongtrack_wavevortex_extension_tests
        PRIVATE
            AlongTrack::WaveVortexExtension
            WaveVortex::ReferenceFFT
    )
    target_include_directories(alongtrack_wavevortex_extension_tests PRIVATE
        "${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}/CompiledKernel/adapters/reference"
    )
    target_compile_definitions(alongtrack_wavevortex_extension_tests PRIVATE
        ATS_WAVEVORTEX_CHECKPOINT_PATH="${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/tests/fixtures/root-hydrostatic.nc"
        ATS_WAVEVORTEX_MATLAB_REFERENCE_PATH="${PROJECT_SOURCE_DIR}/cpp/tests/data/wavevortex_matlab_reference.csv"
    )
    set_target_properties(alongtrack_wavevortex_extension_tests PROPERTIES
        CXX_EXTENSIONS OFF)
    alongtrack_enable_warnings(alongtrack_wavevortex_extension_tests)

    add_test(NAME alongtrack_wavevortex_extension
        COMMAND alongtrack_wavevortex_extension_tests)

    add_executable(alongtrack_wavevortex_extension_routing_tests
        tests/test_wavevortex_extension_routing.cpp
    )
    target_compile_features(alongtrack_wavevortex_extension_routing_tests
        PRIVATE cxx_std_17)
    target_link_libraries(alongtrack_wavevortex_extension_routing_tests
        PRIVATE
            AlongTrack::WaveVortexExtension
            WaveVortex::ReferenceFFT
    )
    target_include_directories(
        alongtrack_wavevortex_extension_routing_tests PRIVATE
        "${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}/CompiledKernel/adapters/reference"
    )
    set_target_properties(alongtrack_wavevortex_extension_routing_tests
        PROPERTIES CXX_EXTENSIONS OFF)
    alongtrack_enable_warnings(
        alongtrack_wavevortex_extension_routing_tests)
    add_test(NAME alongtrack_wavevortex_extension_routing
        COMMAND alongtrack_wavevortex_extension_routing_tests)

    add_test(NAME alongtrack_wavevortex_extension_source_policy
        COMMAND ${CMAKE_COMMAND}
            -DATS_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DATS_WAVEVORTEX_SOURCE_DIR=${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_extension_source_policy.cmake
    )
endif()
