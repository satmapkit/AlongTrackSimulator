add_library(alongtrack_wavevortex_extension STATIC
    src/wavevortex_extension.cpp
)
add_library(AlongTrack::WaveVortexExtension ALIAS
    alongtrack_wavevortex_extension
)

# Newer CMake versions can de-duplicate the explicit source-composition link
# list on linkers that rescan archives automatically. Older supported CMake
# versions retain their established ordering behavior.
if(POLICY CMP0156)
    cmake_policy(SET CMP0156 NEW)
endif()

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
alongtrack_enable_sanitizers(alongtrack_wavevortex_extension)

# Production source-linked runner. The executable owns catalog composition;
# all argument parsing, model inspection, integration, output routing, and
# persistence remain in WaveVortex::Runner.
add_executable(alongtrack-wave-vortex-run
    app/AlongTrackWaveVortexRunMain.cpp
)
target_compile_features(alongtrack-wave-vortex-run PRIVATE cxx_std_17)
target_link_libraries(alongtrack-wave-vortex-run
    PRIVATE
        AlongTrack::PortableCore
        AlongTrack::WaveVortexExtension
        WaveVortex::PortableRuntime
        WaveVortex::Runner
)
set_target_properties(alongtrack-wave-vortex-run PROPERTIES
    CXX_EXTENSIONS OFF
)
if(WV_RUNTIME_ENABLE_NATIVE_FFTW)
    # WaveVortex::Runner owns provider selection and linkage. Mirror the base
    # executable's runtime search path so the source-linked caller reaches the
    # same pinned native provider without copying provider logic.
    set_target_properties(alongtrack-wave-vortex-run PROPERTIES
        BUILD_RPATH "${WV_RUNTIME_FFTW_ROOT}/lib"
    )
endif()
alongtrack_enable_warnings(alongtrack-wave-vortex-run)
alongtrack_enable_sanitizers(alongtrack-wave-vortex-run)

if(BUILD_TESTING)
    if(NOT TARGET wave-vortex-run)
        message(FATAL_ERROR
            "WaveVortex extension tests require the source-checkout wave-vortex-run target")
    endif()
    if(NOT EXISTS
       "${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/third_party/nlohmann/json.hpp")
        message(FATAL_ERROR
            "WaveVortex extension tests require the source-checkout JSON headers")
    endif()

    add_test(NAME alongtrack_wavevortex_runner_composition
        COMMAND ${CMAKE_COMMAND}
            -DATS_RUNNER=$<TARGET_FILE:alongtrack-wave-vortex-run>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_runner_composition.cmake
    )

    add_executable(alongtrack_wavevortex_runner_end_to_end_tests
        tests/test_source_linked_runner.cpp
    )
    target_compile_features(alongtrack_wavevortex_runner_end_to_end_tests
        PRIVATE cxx_std_17)
    target_include_directories(
        alongtrack_wavevortex_runner_end_to_end_tests SYSTEM PRIVATE
        "${ALONGTRACK_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/third_party"
    )
    target_link_libraries(alongtrack_wavevortex_runner_end_to_end_tests
        PRIVATE
            AlongTrack::WaveVortexExtension
            WaveVortex::PortableRuntime
    )
    # The dependency's imported NetCDF target is directory-scoped. Resolve the
    # same source dependency here so this test can inspect persisted values.
    find_package(netCDF CONFIG QUIET)
    if(TARGET netCDF::netcdf)
        target_link_libraries(
            alongtrack_wavevortex_runner_end_to_end_tests PRIVATE
            netCDF::netcdf)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(ALONGTRACK_TEST_NETCDF REQUIRED IMPORTED_TARGET
            netcdf)
        target_link_libraries(
            alongtrack_wavevortex_runner_end_to_end_tests PRIVATE
            PkgConfig::ALONGTRACK_TEST_NETCDF)
    endif()
    target_compile_definitions(
        alongtrack_wavevortex_runner_end_to_end_tests PRIVATE
        ATS4_BASE_RUNNER="$<TARGET_FILE:wave-vortex-run>"
        ATS4_EXTENDED_RUNNER="$<TARGET_FILE:alongtrack-wave-vortex-run>"
        ATS4_REPEATING_REQUEST="${PROJECT_SOURCE_DIR}/cpp/tests/data/ats4/matlab-authored-repeating-request.json"
        ATS4_GEODETIC_REQUEST="${PROJECT_SOURCE_DIR}/cpp/tests/data/ats4/matlab-authored-geodetic-request.json"
    )
    set_target_properties(alongtrack_wavevortex_runner_end_to_end_tests
        PROPERTIES CXX_EXTENSIONS OFF)
    alongtrack_enable_warnings(
        alongtrack_wavevortex_runner_end_to_end_tests)
    alongtrack_enable_sanitizers(
        alongtrack_wavevortex_runner_end_to_end_tests)
    add_dependencies(alongtrack_wavevortex_runner_end_to_end_tests
        wave-vortex-run
        alongtrack-wave-vortex-run
    )
    add_test(NAME alongtrack_wavevortex_runner_end_to_end
        COMMAND alongtrack_wavevortex_runner_end_to_end_tests)

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
    alongtrack_enable_sanitizers(alongtrack_wavevortex_extension_tests)

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
    alongtrack_enable_sanitizers(
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
