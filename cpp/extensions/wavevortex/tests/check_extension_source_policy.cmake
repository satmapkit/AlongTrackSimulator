file(GLOB_RECURSE extension_sources
    "${ATS_SOURCE_DIR}/cpp/extensions/wavevortex/include/*.h"
    "${ATS_SOURCE_DIR}/cpp/extensions/wavevortex/include/*.hpp"
    "${ATS_SOURCE_DIR}/cpp/extensions/wavevortex/src/*.c"
    "${ATS_SOURCE_DIR}/cpp/extensions/wavevortex/src/*.cc"
    "${ATS_SOURCE_DIR}/cpp/extensions/wavevortex/src/*.cpp"
)

if(NOT extension_sources)
    message(FATAL_ERROR "The WaveVortex extension target has no production sources")
endif()

set(found_explicit_registration FALSE)
foreach(source IN LISTS extension_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "registerAlongTrackExtensions")
        set(found_explicit_registration TRUE)
    endif()
    if(contents MATCHES
       "NetCDF|netcdf|nc_[a-zA-Z]|WVOutputSink|WVOutputDriver|WVOutputRoute|dlopen|LoadLibrary|__attribute__[^\n]*constructor|DllMain")
        message(FATAL_ERROR
            "WaveVortex extension source ${source} contains a forbidden sink, routing, persistence, or dynamic-plugin dependency")
    endif()
endforeach()

if(NOT found_explicit_registration)
    message(FATAL_ERROR
        "WaveVortex extension does not expose explicit catalog registration")
endif()

file(READ "${ATS_SOURCE_DIR}/cpp/extensions/wavevortex/CMakeLists.txt"
     extension_cmake)
if(extension_cmake MATCHES "install[ \t\r\n]*\\(|export[ \t\r\n]*\\(")
    message(FATAL_ERROR
        "ATS #3 must not install or export a compiled WaveVortex extension")
endif()

if(ATS_WAVEVORTEX_SOURCE_DIR)
    file(GLOB_RECURSE wavevortex_runtime_sources
        "${ATS_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/include/*.h"
        "${ATS_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/include/*.hpp"
        "${ATS_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/src/*.c"
        "${ATS_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/src/*.cc"
        "${ATS_WAVEVORTEX_SOURCE_DIR}/PortableRuntime/src/*.cpp"
    )
    foreach(source IN LISTS wavevortex_runtime_sources)
        file(READ "${source}" contents)
        if(contents MATCHES "AlongTrackSimulator|WVAlongTrack")
            message(FATAL_ERROR
                "WaveVortexModel runtime source ${source} depends on AlongTrackSimulator")
        endif()
    endforeach()
endif()
